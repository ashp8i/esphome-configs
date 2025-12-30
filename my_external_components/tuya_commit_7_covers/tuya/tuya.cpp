#include "tuya.h"
#include "esphome/components/network/util.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya";
static const int COMMAND_DELAY = 10;
static const int RECEIVE_TIMEOUT = 300;
static const int MAX_RETRIES = 5;

void Tuya::setup() {
  // Only use the standard interval if NOT in our custom keep-alive mode
  if (!this->low_power_mode_ && !this->low_power_keep_alive_) {
    this->set_interval("heartbeat", 15000, [this] {
      this->send_empty_command_(TuyaCommandType::HEARTBEAT);
    });
  }

  if (this->status_pin_ != nullptr) {
    this->status_pin_->digital_write(false);
  }
}

void Tuya::loop() {
  // 1. Always process incoming UART data
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }

  // 2. Passive Heartbeat Logic for Low Power Keep Alive
  // This bypasses the state machine to keep the MCU from falling asleep.
  if (this->low_power_keep_alive_) {
    uint32_t now = millis();
    // Use a 15-second interval
    if (now - this->last_heartbeat_timestamp_ > 15000) {
      this->send_empty_command_(TuyaCommandType::HEARTBEAT);
      this->last_heartbeat_timestamp_ = now;
    }
  }

  // 3. Process the outgoing queue (respecting the COMMAND_DELAY)
  this->process_command_queue_();
}

void Tuya::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya:");
  if (this->init_state_ != TuyaInitState::INIT_DONE) {
    if (this->init_failed_) {
      ESP_LOGCONFIG(TAG, "  Initialization failed. Current init_state: %u", static_cast<uint8_t>(this->init_state_));
    } else {
      ESP_LOGCONFIG(TAG, "  Configuration will be reported when setup is complete. Current init_state: %u",
                    static_cast<uint8_t>(this->init_state_));
    }
    ESP_LOGCONFIG(TAG, "  If no further output is received, confirm that this is a supported Tuya device.");
    return;
  }
  for (auto &info : this->datapoints_) {
    if (info.type == TuyaDatapointType::RAW) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: raw (value: %s)", info.id, format_hex_pretty(info.value_raw).c_str());
    } else if (info.type == TuyaDatapointType::BOOLEAN) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: switch (value: %s)", info.id, ONOFF(info.value_bool));
    } else if (info.type == TuyaDatapointType::INTEGER) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: int value (value: %d)", info.id, info.value_int);
    } else if (info.type == TuyaDatapointType::STRING) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: string value (value: %s)", info.id, info.value_string.c_str());
    } else if (info.type == TuyaDatapointType::ENUM) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: enum (value: %d)", info.id, info.value_enum);
    } else if (info.type == TuyaDatapointType::BITMASK) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: bitmask (value: %" PRIx32 ")", info.id, info.value_bitmask);
    } else {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: unknown", info.id);
    }
  }
  if ((this->status_pin_reported_ != -1) || (this->reset_pin_reported_ != -1)) {
    ESP_LOGCONFIG(TAG, "  GPIO Configuration: status: pin %d, reset: pin %d", this->status_pin_reported_,
                  this->reset_pin_reported_);
  }
  LOG_PIN("  Status Pin: ", this->status_pin_);
  ESP_LOGCONFIG(TAG, "  Product: '%s'", this->product_.c_str());
}

// bool Tuya::validate_message_() {
//   if (this->rx_message_.size() < 7)
//     return true; // Not enough data to even check length

//   auto *data = &this->rx_message_[0];
//   uint16_t length = (uint16_t(data[4]) << 8) | (uint16_t(data[5]));
//   uint32_t total_expected_size = length + 7;

//   if (this->rx_message_.size() < total_expected_size)
//     return true; // Still waiting for the full payload

//   // We have a full packet. Now check the checksum.
//   uint8_t rx_checksum = data[total_expected_size - 1];
//   uint8_t calc_checksum = 0;
//   for (uint32_t i = 0; i < total_expected_size - 1; i++) {
//     calc_checksum += data[i];
//   }

//   if (rx_checksum != calc_checksum) {
//     ESP_LOGW(TAG, "Tuya Checksum Fail: Expected %02X, got %02X", rx_checksum, calc_checksum);
//     // Return false so handle_char_ knows to erase the first byte and try again
//     return false;
//   }

//   // Valid Packet Found!
//   uint8_t version = data[2];
//   uint8_t command = data[3];
//   const uint8_t *message_data = data + 6;

//   ESP_LOGV(TAG, "Received Tuya: CMD=0x%02X VERSION=%u DATA=[%s]",
//            command, version, format_hex_pretty(message_data, length).c_str());

//   this->handle_command_(command, version, message_data, length);

//   return true; // Success
// }

bool Tuya::validate_message_() {
  uint32_t at = this->rx_message_.size() - 1;
  auto *data = &this->rx_message_[0];
  uint8_t new_byte = data[at];

  // Byte 0 & 1: Header (Standard 0x55 0xAA)
  if (at == 0) return new_byte == 0x55;
  if (at == 1) return new_byte == 0xAA;
  if (at == 2) return true; // Version
  if (at == 3) return true; // Command

  // Byte 4 & 5: Length
  if (at <= 5) return true;

  uint16_t length = (uint16_t(data[4]) << 8) | (uint16_t(data[5]));

  // Check 1: Wait until we have the full payload + 1 byte for checksum
  if (at - 6 < length) {
    return true;
  }

  // Check 2: Checksum Validation
  uint8_t rx_checksum = new_byte;
  uint8_t calc_checksum = 0;
  // Tuya checksum is the sum of all bytes except the checksum itself, mod 256
  for (uint32_t i = 0; i < 6 + length; i++) {
    calc_checksum += data[i];
  }

  if (rx_checksum != calc_checksum) {
    ESP_LOGW(TAG, "Tuya Received invalid message checksum %02X != %02X", rx_checksum, calc_checksum);
    return false; // This triggers rx_message.clear() in handle_char_
  }

  // --- VALID MESSAGE REACHED ---
  uint8_t version = data[2];
  uint8_t command = data[3];
  const uint8_t *message_data = data + 6;

  // Change logging to Debug so we can see the "Conversation" clearly
  ESP_LOGD(TAG, "Received Tuya: CMD=0x%02X VERSION=%u DATA=[%s]",
           command, version, format_hex_pretty(message_data, length).c_str());

  this->handle_command_(command, version, message_data, length);

  // Return false to reset buffer for the next message
  return false;
}

void Tuya::handle_char_(uint8_t c) {
  this->rx_message_.push_back(c);

  if (this->raw_trace_) {
    // Only log raw bytes if we are struggling to find a header
    if (this->rx_message_.size() < 2) {
       ESP_LOGD(TAG, "RX Raw: %02X", c);
    }
  }

  while (this->rx_message_.size() >= 2 && (this->rx_message_[0] != 0x55 || this->rx_message_[1] != 0xAA)) {
    if (this->raw_trace_) {
       ESP_LOGV(TAG, "Dropping non-header byte: %02X", this->rx_message_[0]);
    }
    this->rx_message_.erase(this->rx_message_.begin());
  }

  // 2. Length Check & Validation
  if (this->rx_message_.size() >= 7) {
    if (!this->validate_message_()) {
      // Checksum failed? Erase the "false" header and keep searching.
      this->rx_message_.erase(this->rx_message_.begin());
    } else {
      // Success? validate_message_ already called handle_command_, so clear it.
      this->rx_message_.clear();
    }
  }
}

void Tuya::handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
  TuyaCommandType command_type = static_cast<TuyaCommandType>(command);

  if (this->ignore_init_ && this->init_state_ != TuyaInitState::INIT_DONE) {
      // If we see literally anything valid from the MCU, we are 'Done' enough to work.
      this->init_state_ = TuyaInitState::INIT_DONE;
      if (!this->silent_init_) {
        ESP_LOGD(TAG, "Link Alive: Initialization bypassed via command 0x%02X", command);
      }
  }

  // --- ASYNC QUEUE CLEANUP ---
  if (this->expected_response_.has_value()) {
    // Logic: If we got the EXACT response OR if we are in ignore_init and got ANY data (0x05/0x08/0x07)
    bool is_match = (this->expected_response_ == command_type);
    bool is_passive_ack = (this->ignore_init_ && (command_type == TuyaCommandType::DATAPOINT_REPORT_ASYNC ||
                                                  command_type == TuyaCommandType::DATAPOINT_QUERY));

    if (is_match || is_passive_ack) {
      this->expected_response_.reset();
      if (!this->command_queue_.empty()) {
          this->command_queue_.pop_front();
      }
      this->init_retries_ = 0;
    }
  }

  switch (command_type) {
    // case TuyaCommandType::HEARTBEAT:
    //   ESP_LOGV(TAG, "MCU Heartbeat (0x%02X)", buffer[0]);
    //   this->protocol_version_ = version;
    //   if (buffer[0] == 0) {
    //     ESP_LOGI(TAG, "MCU restarted");
    //     this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
    //   }
    //   if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT) {
    //     this->init_state_ = TuyaInitState::INIT_PRODUCT;
    //     this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
    //   }
    //   break;
    case TuyaCommandType::HEARTBEAT:
      ESP_LOGV(TAG, "MCU Heartbeat Received (Payload: 0x%02X)", buffer[0]);
      this->protocol_version_ = version;

      // REASONABLE CHECK: If we get ANY heartbeat, the link is alive.
      // We move to INIT_PRODUCT even if the MCU says it's still 'initializing' (0x01)
      if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT) {
        ESP_LOGD(TAG, "Link established, proceeding to Product Query...");
        this->init_state_ = TuyaInitState::INIT_PRODUCT;
        this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
      }
      break;
    case TuyaCommandType::PRODUCT_QUERY: {
      // check it is a valid string made up of printable characters
      bool valid = true;
      for (size_t i = 0; i < len; i++) {
        if (!std::isprint(static_cast<unsigned char>(buffer[i]))) {
          valid = false;
          break;
        }
      }
      if (valid) {
        this->product_ = std::string(reinterpret_cast<const char *>(buffer), len);
      } else {
        this->product_ = R"({"p":"INVALID"})";
      }
      if (this->init_state_ == TuyaInitState::INIT_PRODUCT) {
        this->init_state_ = TuyaInitState::INIT_CONF;
        this->send_empty_command_(TuyaCommandType::CONF_QUERY);
      }
      break;
    }
    case TuyaCommandType::CONF_QUERY: {
      if (len >= 2) {
        this->status_pin_reported_ = buffer[0];
        this->reset_pin_reported_ = buffer[1];
      }
      if (this->init_state_ == TuyaInitState::INIT_CONF) {
        // If mcu returned status gpio, then we can omit sending wifi state
        if (this->status_pin_reported_ != -1) {
          this->init_state_ = TuyaInitState::INIT_DATAPOINT;
          this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
          bool is_pin_equals =
              this->status_pin_ != nullptr && this->status_pin_->get_pin() == this->status_pin_reported_;
          // Configure status pin toggling (if reported and configured) or WIFI_STATE periodic send
          if (is_pin_equals) {
            ESP_LOGV(TAG, "Configured status pin %i", this->status_pin_reported_);
            this->set_interval("wifi", 1000, [this] { this->set_status_pin_(); });
          } else {
            ESP_LOGW(TAG, "Supplied status_pin does not equals the reported pin %i. TuyaMcu will work in limited mode.",
                     this->status_pin_reported_);
          }
        } else {
          this->init_state_ = TuyaInitState::INIT_WIFI;
          ESP_LOGV(TAG, "Configured WIFI_STATE periodic send");
          this->set_interval("wifi", 1000, [this] { this->send_wifi_status_(); });
        }
      }
      break;
    }
    case TuyaCommandType::WIFI_STATE:
      if (this->init_state_ == TuyaInitState::INIT_WIFI) {
        this->init_state_ = TuyaInitState::INIT_DATAPOINT;
        this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
      }
      break;
    case TuyaCommandType::WIFI_SELECT:
    case TuyaCommandType::WIFI_RESET: {
      // Logic: If user forced V0 mode OR auto-detected low_power,
      // treat 0x05 as data, not a WiFi command.
      if (this->force_low_power_reporting_ || this->low_power_mode_) {
        ESP_LOGD(TAG, "V0/Low-Power DP Report (0x05) received");
        this->handle_datapoints_(buffer, len);
        this->init_state_ = TuyaInitState::INIT_DONE;
      } else {
        const bool is_select = (command_type == TuyaCommandType::WIFI_SELECT);

        // Send WIFI_SELECT/WIFI_RESET ACK
        TuyaCommand ack;
      ack.cmd = is_select ? TuyaCommandType::WIFI_SELECT : TuyaCommandType::WIFI_RESET;
      ack.payload.clear();
      this->send_command_(ack);

        // Establish pairing mode for correct first WIFI_STATE byte
      uint8_t first = 0x00;
      const char *mode_str = "EZ";
      if (is_select && buffer[0] == 0x01) {
        first = 0x01;
        mode_str = "AP";
      }
        // Send WIFI_STATE sequence
      TuyaCommand st;
      st.cmd = TuyaCommandType::WIFI_STATE;
      st.payload.resize(1);
      st.payload[0] = first;
      this->send_command_(st);
      st.payload[0] = 0x02;
      this->send_command_(st);
      st.payload[0] = 0x03;
      this->send_command_(st);
      st.payload[0] = 0x04;
      this->send_command_(st);

        ESP_LOGI(TAG, "%s received (%s), replied with WIFI_STATE confirming connection established",
                is_select ? "WIFI_SELECT" : "WIFI_RESET", mode_str);
      }
      break;
    }
    case TuyaCommandType::DATAPOINT_DELIVER:
      break;
    case TuyaCommandType::DATAPOINT_REPORT_ASYNC:
      // Auto-detect whether this is a low-power device (only valid after init)
      if (!this->low_power_mode_ && this->init_state_ == TuyaInitState::INIT_DONE) {
        static uint32_t last_dp_time = 0;
        static uint8_t dp_count = 0;
        uint32_t now = millis();

        // Only consider recent bursts
        if ((now - last_dp_time) < 30000) {
          dp_count++;
        } else {
          dp_count = 1;  // reset count if spaced out
        }
        last_dp_time = now;

        // If we see 3+ async packets in 30s without heartbeat → low power likely
        if (dp_count >= 3) {
          ESP_LOGW(TAG, "Auto-detected Tuya device as low-power (no heartbeat, frequent DP packets)");
          this->low_power_mode_ = true;
          dp_count = 0;  // reset to avoid future triggers
        }
      }

      this->handle_datapoints_(buffer, len);
      break;
    case TuyaCommandType::DATAPOINT_REPORT_SYNC:
      if (this->init_state_ == TuyaInitState::INIT_DATAPOINT) {
        this->init_state_ = TuyaInitState::INIT_DONE;
        this->set_timeout("datapoint_dump", 1000, [this] { this->dump_config(); });
        this->initialized_callback_.call();
      }
      this->handle_datapoints_(buffer, len);
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_REPORT_ACK, .payload = std::vector<uint8_t>{0x01}});
      break;
    case TuyaCommandType::DATAPOINT_QUERY:
      if (this->handle_historical_ && len > 0) {
        // This activates the helper you added
        this->process_record_report_(buffer, len);
      } else {
        // Optional: Keep a log for debugging covers that use 0x08
        ESP_LOGV(TAG, "Standard Datapoint Query (0x08) received");
      }
      break;
    case TuyaCommandType::WIFI_TEST:
      this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WIFI_TEST, .payload = std::vector<uint8_t>{0x00, 0x00}});
      break;
    case TuyaCommandType::WIFI_RSSI:
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::WIFI_RSSI, .payload = std::vector<uint8_t>{get_wifi_rssi_()}});
      break;
    case TuyaCommandType::LOCAL_TIME_QUERY:
#ifdef USE_TIME
      if (this->time_id_ != nullptr) {
        this->send_local_time_();

        if (!this->time_sync_callback_registered_) {
          // tuya mcu supports time, so we let them know when our time changed
          this->time_id_->add_on_time_sync_callback([this] { this->send_local_time_(); });
          this->time_sync_callback_registered_ = true;
        }
      } else
#endif
      {
        ESP_LOGW(TAG, "LOCAL_TIME_QUERY is not handled because time is not configured");
      }
      break;
    case TuyaCommandType::VACUUM_MAP_UPLOAD:
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::VACUUM_MAP_UPLOAD, .payload = std::vector<uint8_t>{0x01}});
      ESP_LOGW(TAG, "Vacuum map upload requested, responding that it is not enabled.");
      break;
    case TuyaCommandType::GET_NETWORK_STATUS: {
      uint8_t wifi_status = this->get_wifi_status_code_();

      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::GET_NETWORK_STATUS, .payload = std::vector<uint8_t>{wifi_status}});
      ESP_LOGV(TAG, "Network status requested, reported as %i", wifi_status);
      break;
    }
    case TuyaCommandType::EXTENDED_SERVICES: {
      uint8_t subcommand = buffer[0];
      switch ((TuyaExtendedServicesCommandType) subcommand) {
        case TuyaExtendedServicesCommandType::RESET_NOTIFICATION: {
          this->send_command_(
              TuyaCommand{.cmd = TuyaCommandType::EXTENDED_SERVICES,
                          .payload = std::vector<uint8_t>{
                              static_cast<uint8_t>(TuyaExtendedServicesCommandType::RESET_NOTIFICATION), 0x00}});
          ESP_LOGV(TAG, "Reset status notification enabled");
          break;
        }
        case TuyaExtendedServicesCommandType::MODULE_RESET: {
          ESP_LOGE(TAG, "EXTENDED_SERVICES::MODULE_RESET is not handled");
          break;
        }
        case TuyaExtendedServicesCommandType::UPDATE_IN_PROGRESS: {
          ESP_LOGE(TAG, "EXTENDED_SERVICES::UPDATE_IN_PROGRESS is not handled");
          break;
        }
        default:
          ESP_LOGE(TAG, "Invalid extended services subcommand (0x%02X) received", subcommand);
      }
      break;
    }
    default:
      ESP_LOGE(TAG, "Invalid command (0x%02X) received", command);
  }
}

// void Tuya::handle_datapoints_(const uint8_t *buffer, size_t len) {
//   while (len >= 4) {
//     TuyaDatapoint datapoint{};
//     datapoint.id = buffer[0];
//     datapoint.type = (TuyaDatapointType) buffer[1];
//     datapoint.value_uint = 0;

//     size_t data_size = (buffer[2] << 8) + buffer[3];
//     const uint8_t *data = buffer + 4;
//     size_t data_len = len - 4;
//     if (data_size > data_len) {
//       ESP_LOGW(TAG, "Datapoint %u is truncated and cannot be parsed (%zu > %zu)", datapoint.id, data_size, data_len);
//       return;
//     }

//     datapoint.len = data_size;

//     switch (datapoint.type) {
//       case TuyaDatapointType::RAW:
//         datapoint.value_raw = std::vector<uint8_t>(data, data + data_size);
//         ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, format_hex_pretty(datapoint.value_raw).c_str());
//         break;
//       case TuyaDatapointType::BOOLEAN:
//         if (data_size != 1) {
//           ESP_LOGW(TAG, "Datapoint %u has bad boolean len %zu", datapoint.id, data_size);
//           return;
//         }
//         datapoint.value_bool = data[0];
//         ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, ONOFF(datapoint.value_bool));
//         break;
//       case TuyaDatapointType::INTEGER:
//         if (data_size != 4) {
//           ESP_LOGW(TAG, "Datapoint %u has bad integer len %zu", datapoint.id, data_size);
//           return;
//         }
//         datapoint.value_uint = encode_uint32(data[0], data[1], data[2], data[3]);
//         ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_int);
//         break;
//       case TuyaDatapointType::STRING:
//         datapoint.value_string = std::string(reinterpret_cast<const char *>(data), data_size);
//         ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, datapoint.value_string.c_str());
//         break;
//       case TuyaDatapointType::ENUM:
//         if (data_size != 1) {
//           ESP_LOGW(TAG, "Datapoint %u has bad enum len %zu", datapoint.id, data_size);
//           return;
//         }
//         datapoint.value_enum = data[0];
//         ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_enum);
//         break;
//       case TuyaDatapointType::BITMASK:
//         switch (data_size) {
//           case 1:
//             datapoint.value_bitmask = encode_uint32(0, 0, 0, data[0]);
//             break;
//           case 2:
//             datapoint.value_bitmask = encode_uint32(0, 0, data[0], data[1]);
//             break;
//           case 4:
//             datapoint.value_bitmask = encode_uint32(data[0], data[1], data[2], data[3]);
//             break;
//           default:
//             ESP_LOGW(TAG, "Datapoint %u has bad bitmask len %zu", datapoint.id, data_size);
//             return;
//         }
//         ESP_LOGD(TAG, "Datapoint %u update to %#08" PRIX32, datapoint.id, datapoint.value_bitmask);
//         break;
//       default:
//         ESP_LOGW(TAG, "Datapoint %u has unknown type %#02hhX", datapoint.id, static_cast<uint8_t>(datapoint.type));
//         return;
//     }

//     len -= data_size + 4;
//     buffer = data + data_size;

//     // drop update if datapoint is in ignore_mcu_datapoint_update list
//     bool skip = false;
//     for (auto i : this->ignore_mcu_update_on_datapoints_) {
//       if (datapoint.id == i) {
//         ESP_LOGV(TAG, "Datapoint %u found in ignore_mcu_update_on_datapoints list, dropping MCU update", datapoint.id);
//         skip = true;
//         break;
//       }
//     }
//     if (skip)
//       continue;

//     // 1. Dynamic Discovery (Already robust in your code)
//     bool found = false;
//     for (auto &other : this->datapoints_) {
//       if (other.id == datapoint.id) {
//         // Optimization: Only update and trigger if the value actually CHANGED
//         // This prevents the S3 from wasting power/WiFi cycles on redundant reports
//         if (other.value_uint == datapoint.value_uint &&
//             other.value_raw == datapoint.value_raw &&
//             !this->force_low_power_reporting_) { // Unless forced by our custom flag
//            found = true;
//            skip = true;
//            break;
//         }
//         other = datapoint;
//         found = true;
//         break;
//       }
//     }

//     if (!found) {
//       this->datapoints_.push_back(datapoint);
//       ESP_LOGI(TAG, "New Datapoint %u discovered via passive report", datapoint.id);
//     }

//     if (skip) continue;

//     // 2. Trigger Listeners (The bridge to the Cover/Sensors)
//     for (auto &listener : this->listeners_) {
//       if (listener.datapoint_id == datapoint.id)
//         listener.on_datapoint(datapoint);
//     }
//   }
// }

// void Tuya::handle_datapoints_(const uint8_t *buffer, size_t len) {
//   while (len >= 4) {
//     TuyaDatapoint datapoint{};
//     datapoint.id = buffer[0];
//     datapoint.type = (TuyaDatapointType) buffer[1];
//     datapoint.value_uint = 0;

//     size_t data_size = (buffer[2] << 8) + buffer[3];
//     const uint8_t *data = buffer + 4;
//     size_t data_len = len - 4;
//     if (data_size > data_len) {
//       ESP_LOGW(TAG, "Datapoint %u is truncated and cannot be parsed (%zu > %zu)", datapoint.id, data_size, data_len);
//       return;
//     }

//     datapoint.len = data_size;

//     switch (datapoint.type) {
//       case TuyaDatapointType::RAW:
//         datapoint.value_raw = std::vector<uint8_t>(data, data + data_size);
//         ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, format_hex_pretty(datapoint.value_raw).c_str());
//         break;
//       case TuyaDatapointType::BOOLEAN:
//         if (data_size != 1) {
//           ESP_LOGW(TAG, "Datapoint %u has bad boolean len %zu", datapoint.id, data_size);
//           return;
//         }
//         datapoint.value_bool = data[0];
//         ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, ONOFF(datapoint.value_bool));
//         break;
//       case TuyaDatapointType::INTEGER:
//         if (data_size != 4) {
//           ESP_LOGW(TAG, "Datapoint %u has bad integer len %zu", datapoint.id, data_size);
//           return;
//         }
//         datapoint.value_uint = encode_uint32(data[0], data[1], data[2], data[3]);
//         ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_int);
//         break;
//       case TuyaDatapointType::STRING:
//         datapoint.value_string = std::string(reinterpret_cast<const char *>(data), data_size);
//         ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, datapoint.value_string.c_str());
//         break;
//       case TuyaDatapointType::ENUM:
//         if (data_size != 1) {
//           ESP_LOGW(TAG, "Datapoint %u has bad enum len %zu", datapoint.id, data_size);
//           return;
//         }
//         datapoint.value_enum = data[0];
//         ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_enum);
//         break;
//       case TuyaDatapointType::BITMASK:
//         switch (data_size) {
//           case 1:
//             datapoint.value_bitmask = encode_uint32(0, 0, 0, data[0]);
//             break;
//           case 2:
//             datapoint.value_bitmask = encode_uint32(0, 0, data[0], data[1]);
//             break;
//           case 4:
//             datapoint.value_bitmask = encode_uint32(data[0], data[1], data[2], data[3]);
//             break;
//           default:
//             ESP_LOGW(TAG, "Datapoint %u has bad bitmask len %zu", datapoint.id, data_size);
//             return;
//         }
//         ESP_LOGD(TAG, "Datapoint %u update to %#08" PRIX32, datapoint.id, datapoint.value_bitmask);
//         break;
//       default:
//         ESP_LOGW(TAG, "Datapoint %u has unknown type %#02hhX", datapoint.id, static_cast<uint8_t>(datapoint.type));
//         return;
//     }

//     len -= data_size + 4;
//     buffer = data + data_size;

//     // drop update if datapoint is in ignore_mcu_datapoint_update list
//     bool skip = false;
//     for (auto i : this->ignore_mcu_update_on_datapoints_) {
//       if (datapoint.id == i) {
//         ESP_LOGV(TAG, "Datapoint %u found in ignore_mcu_update_on_datapoints list, dropping MCU update", datapoint.id);
//         skip = true;
//         break;
//       }
//     }
//     if (skip)
//       continue;

//     // Update internal datapoints
//     bool found = false;
//     for (auto &other : this->datapoints_) {
//       if (other.id == datapoint.id) {
//         other = datapoint;
//         found = true;
//       }
//     }
//     if (!found) {
//       this->datapoints_.push_back(datapoint);
//     }

//     // Run through listeners
//     for (auto &listener : this->listeners_) {
//       if (listener.datapoint_id == datapoint.id)
//         listener.on_datapoint(datapoint);
//     }
//   }
// }
void Tuya::handle_datapoints_(const uint8_t *buffer, size_t len) {
  while (len >= 4) {
    TuyaDatapoint datapoint{};
    datapoint.id = buffer[0];
    datapoint.type = (TuyaDatapointType) buffer[1];
    datapoint.value_uint = 0;

    size_t data_size = (buffer[2] << 8) + buffer[3];
    const uint8_t *data = buffer + 4;
    size_t data_len = len - 4;
    if (data_size > data_len) {
      ESP_LOGW(TAG, "Datapoint %u is truncated and cannot be parsed (%zu > %zu)", datapoint.id, data_size, data_len);
      return;
    }

    datapoint.len = data_size;

    // --- PARSING SECTION ---
    switch (datapoint.type) {
      case TuyaDatapointType::RAW:
        datapoint.value_raw = std::vector<uint8_t>(data, data + data_size);
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, format_hex_pretty(datapoint.value_raw).c_str());
        break;
      case TuyaDatapointType::BOOLEAN:
        if (data_size != 1) { return; }
        datapoint.value_bool = data[0];
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, ONOFF(datapoint.value_bool));
        break;
      case TuyaDatapointType::INTEGER:
        if (data_size != 4) { return; }
        datapoint.value_uint = encode_uint32(data[0], data[1], data[2], data[3]);
        ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_int);
        break;
      case TuyaDatapointType::STRING:
        datapoint.value_string = std::string(reinterpret_cast<const char *>(data), data_size);
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, datapoint.value_string.c_str());
        break;
      case TuyaDatapointType::ENUM:
        if (data_size != 1) { return; }
        datapoint.value_enum = data[0];
        ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_enum);
        break;
      case TuyaDatapointType::BITMASK:
        // ... (Keep your bitmask switch here)
        break;
      default:
        ESP_LOGW(TAG, "Datapoint %u has unknown type %#02hhX", datapoint.id, static_cast<uint8_t>(datapoint.type));
        return;
    }

    // Move pointers for next iteration
    len -= data_size + 4;
    buffer = data + data_size;

    // --- FILTER SECTION ---
    bool skip = false;
    for (auto i : this->ignore_mcu_update_on_datapoints_) {
      if (datapoint.id == i) {
        skip = true;
        break;
      }
    }
    if (skip) continue;

    // --- THE ELECTRON MICROSCOPE INTEGRATION ---
    bool found = false;
    for (auto &other : this->datapoints_) {
      if (other.id == datapoint.id) {
        other = datapoint;
        found = true;
        break; // Found it, stop looking
      }
    }

    if (!found) {
      // This is a brand new DP discovered during a report
      ESP_LOGI(TAG, "Discovery: MCU reported UNKNOWN Datapoint %u (Type: %u, Len: %zu). Values: %s",
               datapoint.id, static_cast<uint8_t>(datapoint.type), datapoint.len,
               format_hex_pretty(data, data_size).c_str());
      this->datapoints_.push_back(datapoint);
    }

    // --- LISTENER SECTION ---
    for (auto &listener : this->listeners_) {
      if (listener.datapoint_id == datapoint.id)
        listener.on_datapoint(datapoint);
    }
  }
}

// void Tuya::process_command_queue_() {
//   uint32_t now = millis();
//   uint32_t delay = now - this->last_command_timestamp_;

//   // 1. Throttling Gate: Always respect the 100ms gap to prevent serial flooding
//   if (delay < COMMAND_DELAY)
//     return;

//   // 2. The Lock Bypass
//   // If we are in "Ignore Init" mode, we don't care about expected_response_
//   if (!this->command_queue_.empty() && this->rx_message_.empty()) {

//     // In "Dumb/Passive" mode, we never wait for ACKs
//     if (this->ignore_init_ || !this->expected_response_.has_value()) {
//       auto &cmd = this->command_queue_.front();
//       this->send_raw_command_(cmd);
//       this->command_queue_.pop_front();
//       this->last_command_timestamp_ = now;
//     }
//     // Otherwise, handle the V3 "Smart" timeout logic
//     else if (delay > 500) {
//       ESP_LOGV(TAG, "Timeout waiting for 0x%02X, skipping.", static_cast<uint8_t>(*this->expected_response_));
//       this->expected_response_.reset();
//     }
//   }
// }

// void Tuya::process_command_queue_() {
//   uint32_t now = millis();
//   uint32_t delay = now - this->last_command_timestamp_;

//   // 1. Handle RX Timeout (Reset buffer if MCU hanging)
//   if (now - this->last_rx_char_timestamp_ > RECEIVE_TIMEOUT) {
//     this->rx_message_.clear();
//   }

//   // 2. Handle Response Timeout (ACK/Response missing)
//   if (this->expected_response_.has_value() && delay > RECEIVE_TIMEOUT) {
//     this->expected_response_.reset();

//     // If we are still in the Init sequence, handle retries
//     if (this->init_state_ != TuyaInitState::INIT_DONE) {
//       if (++this->init_retries_ >= MAX_RETRIES) {
//         this->init_failed_ = true;
//         ESP_LOGE(TAG, "Initialization failed at init_state %u", static_cast<uint8_t>(this->init_state_));
//         this->command_queue_.erase(this->command_queue_.begin());
//         this->init_retries_ = 0;
//       }
//     } else {
//       // Not in init, just drop the failed command from queue and move on
//       this->command_queue_.erase(this->command_queue_.begin());
//     }
//   }

//   // 3. Dispatch Logic
//   // Only send if:
//   // - Delay met AND Queue not empty AND Not mid-reception AND Not waiting for a response
//   if (delay > COMMAND_DELAY && !this->command_queue_.empty() &&
//       this->rx_message_.empty() && !this->expected_response_.has_value()) {

//     this->send_raw_command_(this->command_queue_.front());
//     this->last_command_timestamp_ = now; // Update timestamp here for accurate spacing

//     // If the command doesn't expect a response (fire-and-forget), remove it immediately
//     if (!this->expected_response_.has_value()) {
//       this->command_queue_.erase(this->command_queue_.begin());
//     }
//   }
// }

// void Tuya::process_command_queue_() {
//   uint32_t now = millis();
//   uint32_t delay = now - this->last_command_timestamp_;

//   // Check 1: Buffer Timeout (Transparency)
//   if (now - this->last_rx_char_timestamp_ > RECEIVE_TIMEOUT && !this->rx_message_.empty()) {
//     ESP_LOGV(TAG, "Cleanup: Clearing stale RX buffer (%zu bytes)", this->rx_message_.size());
//     this->rx_message_.clear();
//   }

//   // Check 2: The "Expectation" Lock (Leaky vs Transparent)
//   if (this->expected_response_.has_value() && delay > RECEIVE_TIMEOUT) {
//     ESP_LOGW(TAG, "Timeout: No response for CMD 0x%02X. State: %u, Retries: %u",
//              static_cast<uint8_t>(this->expected_response_.value()),
//              static_cast<uint8_t>(this->init_state_), this->init_retries_);

//     this->expected_response_.reset();
//     if (this->init_state_ != TuyaInitState::INIT_DONE) {
//       if (++this->init_retries_ >= MAX_RETRIES) {
//         this->init_failed_ = true;
//         ESP_LOGE(TAG, "Infrastructure Failure: Critical timeout at init_state %u", static_cast<uint8_t>(this->init_state_));
//         this->command_queue_.erase(this->command_queue_.begin());
//         this->init_retries_ = 0;
//       }
//     } else {
//       // For non-init commands, just drop and move on
//       ESP_LOGD(TAG, "Queue: Dropping timed-out non-critical command");
//       this->command_queue_.erase(this->command_queue_.begin());
//     }
//   }

//   // Check 3: The "Dunce" Gate (Why aren't we sending?)
//   if (!this->command_queue_.empty() && !this->expected_response_.has_value()) {
//     if (delay <= COMMAND_DELAY) return; // Standard pacing

//     if (!this->rx_message_.empty()) {
//       ESP_LOGV(TAG, "Gate: Command blocked, RX buffer currently busy");
//       return;
//     }

//     ESP_LOGD(TAG, "Infrastructure: Pushing front of queue (Size: %zu)", this->command_queue_.size());
//     this->send_raw_command_(command_queue_.front());

//     if (!this->expected_response_.has_value())
//       this->command_queue_.erase(this->command_queue_.begin());
//   }
// }

void Tuya::process_command_queue_() {
  uint32_t now = millis();
  uint32_t delay = now - this->last_command_timestamp_;

  // WINDOW ADAPTATION:
  // If we are in initialization, give the MCU 1500ms (Passive Mode).
  // If we are finished init, give it 500ms (Active Mode).
  uint32_t current_timeout = (this->init_state_ == TuyaInitState::INIT_DONE) ? 500 : 1500;

  // // 1. Buffer Timeout (Adaptive)
  // if (!this->rx_message_.empty() && (now - this->last_rx_char_timestamp_ > current_timeout)) {
  //   ESP_LOGV(TAG, "Cleanup: Clearing stale RX buffer (%zu bytes)", this->rx_message_.size());
  //   this->rx_message_.clear();
  // }

  // 2. The Expectation Gate (Adaptive)
  if (this->expected_response_.has_value() && delay > current_timeout) {
    // Declare it here so it's available for the whole block
    uint8_t timed_out_cmd = static_cast<uint8_t>(this->expected_response_.value());

    ESP_LOGW(TAG, "Gate Timeout: No response for 0x%02X within %ums", timed_out_cmd, current_timeout);

    this->expected_response_.reset();

    if (this->init_state_ != TuyaInitState::INIT_DONE) {
      if (++this->init_retries_ >= MAX_RETRIES) {
        this->init_failed_ = true;
        ESP_LOGE(TAG, "Infrastructure: Critical stall at init_state %u", static_cast<uint8_t>(this->init_state_));
        if (!this->command_queue_.empty()) this->command_queue_.pop_front();
        this->init_retries_ = 0;
      }
    } else {
      if (!this->command_queue_.empty()) {
        // Now timed_out_cmd is in scope here!
        ESP_LOGV(TAG, "Queue: Dropping unacknowledged command 0x%02X", timed_out_cmd);
        this->command_queue_.pop_front();
      }
    }
  }

  // 3. The Transmission Gate
  if (!this->command_queue_.empty() && !this->expected_response_.has_value()) {
    // Standard Pacing
    if (delay <= COMMAND_DELAY) return;

    // Don't interrupt a potential incoming message
    if (!this->rx_message_.empty()) return;

    // Send the next command in the deque
    ESP_LOGD(TAG, "Infrastructure: Sending 0x%02X (Queue size: %zu)",
             static_cast<uint8_t>(this->command_queue_.front().cmd), this->command_queue_.size());

    this->send_raw_command_(this->command_queue_.front());

    // If the command we just sent doesn't require a response, pop it immediately
    if (!this->expected_response_.has_value()) {
      this->command_queue_.pop_front();
    }
  }
}

// void Tuya::send_raw_command_(TuyaCommand command) {
//   uint8_t len_hi = (uint8_t) (command.payload.size() >> 8);
//   uint8_t len_lo = (uint8_t) (command.payload.size() & 0xFF);
//   uint8_t version = 0;

//   // 1. PRE-CALCULATE CHECKSUM (Minimize CPU work during UART streaming)
//   uint8_t checksum = 0x55 + 0xAA + version + (uint8_t) command.cmd + len_hi + len_lo;
//   for (auto &data : command.payload)
//     checksum += data;

//   this->last_command_timestamp_ = millis();

//   // 2. STATE MANAGEMENT (Set expectations BEFORE physical write)
//   if (!this->low_power_mode_ && !this->ignore_init_) {
//     switch (command.cmd) {
//       case TuyaCommandType::HEARTBEAT:
//       case TuyaCommandType::PRODUCT_QUERY:
//       case TuyaCommandType::CONF_QUERY:
//       case TuyaCommandType::WIFI_STATE:
//       case TuyaCommandType::WIFI_SELECT:
//       case TuyaCommandType::WIFI_RESET:
//         this->expected_response_ = command.cmd;
//         break;
//       case TuyaCommandType::DATAPOINT_DELIVER:
//       case TuyaCommandType::DATAPOINT_QUERY:
//         this->expected_response_ = TuyaCommandType::DATAPOINT_REPORT_ASYNC;
//         break;
//       default:
//         this->expected_response_.reset();
//         break;
//     }
//   } else {
//     this->expected_response_.reset();
//   }

//   // 3. PHYSICAL WRITE (High-speed burst)
//   this->write_array({0x55, 0xAA, version, (uint8_t) command.cmd, len_hi, len_lo});
//   if (!command.payload.empty()) {
//     this->write_array(command.payload.data(), command.payload.size());
//   }
//   this->write_byte(checksum);

//   // 4. LOGGING (Lowest priority - executed only after hardware FIFO is handed the data)
//   if (this->trace_mode_ && !this->raw_trace_) {
//     ESP_LOGI(TAG, "Trace: [Sent 0x%02X] Checksum: 0x%02X Payload: %s",
//              static_cast<uint8_t>(command.cmd),
//              checksum,
//              format_hex_pretty(command.payload).c_str());
//   }
// }

void Tuya::send_raw_command_(TuyaCommand command) {
  uint8_t len_hi = (uint8_t) (command.payload.size() >> 8);
  uint8_t len_lo = (uint8_t) (command.payload.size() & 0xFF);
  uint8_t version = 0; // Or this->protocol_version_ if you prefer

  this->last_command_timestamp_ = millis();

  // Update the "Expected Response" contract based on the command sent
  switch (command.cmd) {
    case TuyaCommandType::HEARTBEAT:
      this->expected_response_ = TuyaCommandType::HEARTBEAT;
      break;
    case TuyaCommandType::PRODUCT_QUERY:
      this->expected_response_ = TuyaCommandType::PRODUCT_QUERY;
      break;
    case TuyaCommandType::CONF_QUERY:
      this->expected_response_ = TuyaCommandType::CONF_QUERY;
      break;
    case TuyaCommandType::DATAPOINT_DELIVER:
    case TuyaCommandType::DATAPOINT_QUERY:
      this->expected_response_ = TuyaCommandType::DATAPOINT_REPORT_ASYNC;
      break;
    default:
      break;
  }

  ESP_LOGV(TAG, "Sending Tuya: CMD=0x%02X VERSION=%u DATA=[%s]",
           static_cast<uint8_t>(command.cmd), version,
           format_hex_pretty(command.payload).c_str());

  this->write_array({0x55, 0xAA, version, (uint8_t) command.cmd, len_hi, len_lo});
  if (!command.payload.empty())
    this->write_array(command.payload.data(), command.payload.size());

  uint8_t checksum = 0x55 + 0xAA + version + (uint8_t) command.cmd + len_hi + len_lo;
  for (auto &data : command.payload)
    checksum += data;
  this->write_byte(checksum);
}

void Tuya::send_command_(const TuyaCommand &command) {
  command_queue_.push_back(command);
  process_command_queue_();
}

void Tuya::send_empty_command_(TuyaCommandType command) {
  send_command_(TuyaCommand{.cmd = command, .payload = std::vector<uint8_t>{}});
}

void Tuya::set_status_pin_() {
  bool is_network_ready = network::is_connected() && remote_is_connected();
  this->status_pin_->digital_write(is_network_ready);
}

uint8_t Tuya::get_wifi_status_code_() {
  uint8_t status = 0x02;

  if (network::is_connected()) {
    status = 0x03;

    // Protocol version 3 also supports specifying when connected to "the cloud"
    if (this->protocol_version_ >= 0x03 && remote_is_connected()) {
      status = 0x04;
    }
  } else {
#ifdef USE_CAPTIVE_PORTAL
    if (captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active()) {
      status = 0x01;
    }
#endif
  };

  return status;
}

uint8_t Tuya::get_wifi_rssi_() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->wifi_rssi();
#endif

  return 0;
}

void Tuya::send_wifi_status_() {
  uint8_t status = this->get_wifi_status_code_();

  if (status == this->wifi_status_) {
    return;
  }

  ESP_LOGD(TAG, "Sending WiFi Status");
  this->wifi_status_ = status;
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WIFI_STATE, .payload = std::vector<uint8_t>{status}});
}

#ifdef USE_TIME
void Tuya::send_local_time_() {
  std::vector<uint8_t> payload;
  ESPTime now = this->time_id_->now();
  if (now.is_valid()) {
    uint8_t year = now.year - 2000;
    uint8_t month = now.month;
    uint8_t day_of_month = now.day_of_month;
    uint8_t hour = now.hour;
    uint8_t minute = now.minute;
    uint8_t second = now.second;
    // Tuya days starts from Monday, esphome uses Sunday as day 1
    uint8_t day_of_week = now.day_of_week - 1;
    if (day_of_week == 0) {
      day_of_week = 7;
    }
    ESP_LOGD(TAG, "Sending local time");
    payload = std::vector<uint8_t>{0x01, year, month, day_of_month, hour, minute, second, day_of_week};
  } else {
    // By spec we need to notify MCU that the time was not obtained if this is a response to a query
    ESP_LOGW(TAG, "Sending missing local time");
    payload = std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::LOCAL_TIME_QUERY, .payload = payload});
}
#endif

void Tuya::set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value) {
  this->set_raw_datapoint_value_(datapoint_id, value, false);
}

void Tuya::set_boolean_datapoint_value(uint8_t datapoint_id, bool value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BOOLEAN, value, 1, false);
}

void Tuya::set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::INTEGER, value, 4, false);
}

void Tuya::set_string_datapoint_value(uint8_t datapoint_id, const std::string &value) {
  this->set_string_datapoint_value_(datapoint_id, value, false);
}

void Tuya::set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::ENUM, value, 1, false);
}

void Tuya::set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BITMASK, value, length, false);
}

void Tuya::force_set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value) {
  this->set_raw_datapoint_value_(datapoint_id, value, true);
}

void Tuya::force_set_boolean_datapoint_value(uint8_t datapoint_id, bool value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BOOLEAN, value, 1, true);
}

void Tuya::force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::INTEGER, value, 4, true);
}

void Tuya::force_set_string_datapoint_value(uint8_t datapoint_id, const std::string &value) {
  this->set_string_datapoint_value_(datapoint_id, value, true);
}

void Tuya::force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::ENUM, value, 1, true);
}

void Tuya::force_set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BITMASK, value, length, true);
}

optional<TuyaDatapoint> Tuya::get_datapoint_(uint8_t datapoint_id) {
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      return datapoint;
  }
  return {};
}

// void Tuya::set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint32_t value,
//                                         uint8_t length, bool forced) {
//   ESP_LOGD(TAG, "Setting datapoint %u to %" PRIu32, datapoint_id, value);

//   optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);

//   if (!datapoint.has_value()) {
//     // In V0/Passive mode, we don't want to stop just because the MCU hasn't 'introduced' itself.
//     if (this->ignore_init_) {
//        ESP_LOGV(TAG, "Datapoint %u unknown, but proceeding due to ignore_initialization", datapoint_id);
//     } else {
//        ESP_LOGW(TAG, "Setting unknown datapoint %u. This might fail if the MCU isn't ready.", datapoint_id);
//     }
//   } else {
//     // Type checking only if we actually have the DP metadata
//     if (datapoint->type != datapoint_type) {
//       ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
//       return;
//     }
//     // Optimization: Don't send if value hasn't changed
//     if (!forced && datapoint->value_uint == value) {
//       ESP_LOGV(TAG, "Not sending unchanged value %" PRIu32 " for DP %u", value, datapoint_id);
//       return;
//     }
//   }

//   std::vector<uint8_t> data;
//   switch (length) {
//     case 4:
//       data.push_back(static_cast<uint8_t>(value >> 24));
//       data.push_back(static_cast<uint8_t>(value >> 16));
//       // fallthrough
//     case 2:
//       data.push_back(static_cast<uint8_t>(value >> 8));
//       // fallthrough
//     case 1:
//       data.push_back(static_cast<uint8_t>(value >> 0));
//       break;
//     default:
//       ESP_LOGE(TAG, "Unexpected datapoint length %u", length);
//       return;
//   }

//   this->send_datapoint_command_(datapoint_id, datapoint_type, data);
// }

// void Tuya::set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint32_t value,
//                                         uint8_t length, bool forced) {
//   ESP_LOGD(TAG, "Setting datapoint %u to %" PRIu32, datapoint_id, value);
//   optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
//   if (!datapoint.has_value()) {
//     ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
//   } else if (datapoint->type != datapoint_type) {
//     ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
//     return;
//   } else if (!forced && datapoint->value_uint == value) {
//     ESP_LOGV(TAG, "Not sending unchanged value");
//     return;
//   }

//   std::vector<uint8_t> data;
//   switch (length) {
//     case 4:
//       data.push_back(value >> 24);
//       data.push_back(value >> 16);
//     case 2:
//       data.push_back(value >> 8);
//     case 1:
//       data.push_back(value >> 0);
//       break;
//     default:
//       ESP_LOGE(TAG, "Unexpected datapoint length %u", length);
//       return;
//   }
//   this->send_datapoint_command_(datapoint_id, datapoint_type, data);
// }

// void Tuya::set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint32_t value,
//                                         uint8_t length, bool forced) {
//   // 1. Check for existing knowledge of this DP
//   optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);

//   if (datapoint.has_value()) {
//     // Validation: Only error out if we KNOW the type is wrong
//     if (datapoint->type != datapoint_type) {
//       ESP_LOGE(TAG, "Type Mismatch for DP %u: expected %u, got %u",
//                datapoint_id, static_cast<uint8_t>(datapoint->type), static_cast<uint8_t>(datapoint_type));
//       return;
//     }

//     // Optimization: Skip if value is identical (unless 'forced' or 'ignore_init' is active)
//     if (!forced && !this->ignore_init_ && datapoint->value_uint == value) {
//       ESP_LOGV(TAG, "DP %u: Value %" PRIu32 " unchanged, skipping", datapoint_id, value);
//       return;
//     }
//   } else {
//     // If we don't know the DP, we warn but DO NOT return if we are in passive/ignore mode
//     if (this->ignore_init_) {
//       ESP_LOGV(TAG, "DP %u unknown, forcing send in passive mode", datapoint_id);
//     } else {
//       ESP_LOGW(TAG, "DP %u unknown, sending anyway (initialization may be incomplete)", datapoint_id);
//     }
//   }

//   ESP_LOGD(TAG, "Setting DP %u to %" PRIu32 " (type %u, len %u)",
//            datapoint_id, value, static_cast<uint8_t>(datapoint_type), length);

//   // 2. Build the Payload
//   std::vector<uint8_t> data;
//   switch (length) {
//     case 4:
//       data.push_back(static_cast<uint8_t>(value >> 24));
//       data.push_back(static_cast<uint8_t>(value >> 16));
//       // fallthrough
//     case 2:
//       data.push_back(static_cast<uint8_t>(value >> 8));
//       // fallthrough
//     case 1:
//       data.push_back(static_cast<uint8_t>(value & 0xFF));
//       break;
//     default:
//       ESP_LOGE(TAG, "Invalid payload length %u for DP %u", length, datapoint_id);
//       return;
//   }

//   this->send_datapoint_command_(datapoint_id, datapoint_type, data);
// }

void Tuya::set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint32_t value,
                                        uint8_t length, bool forced) {
  // LOG EVERYTHING
  ESP_LOGI(TAG, "Direct-Write Request: DP %u, Value %u", datapoint_id, value);

  std::vector<uint8_t> data;
  switch (length) {
    case 4: data.push_back(value >> 24); data.push_back(value >> 16);
    case 2: data.push_back(value >> 8);
    case 1: data.push_back(value & 0xFF); break;
    default: return;
  }

  // Bypass the get_datapoint_ check entirely
  this->send_datapoint_command_(datapoint_id, datapoint_type, data);
}

void Tuya::set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %s", datapoint_id, format_hex_pretty(value).c_str());
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
    ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  } else if (datapoint->type != TuyaDatapointType::RAW) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_raw == value) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }
  this->send_datapoint_command_(datapoint_id, TuyaDatapointType::RAW, value);
}

void Tuya::set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %s", datapoint_id, value.c_str());
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
    ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  } else if (datapoint->type != TuyaDatapointType::STRING) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_string == value) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }
  std::vector<uint8_t> data;
  for (char const &c : value) {
    data.push_back(c);
  }
  this->send_datapoint_command_(datapoint_id, TuyaDatapointType::STRING, data);
}

// void Tuya::send_datapoint_command_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, std::vector<uint8_t> data) {
//   std::vector<uint8_t> buffer;
//   buffer.push_back(datapoint_id);
//   buffer.push_back(static_cast<uint8_t>(datapoint_type));
//   buffer.push_back(data.size() >> 8);
//   buffer.push_back(data.size() >> 0);
//   buffer.insert(buffer.end(), data.begin(), data.end());

//   this->send_command_(TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_DELIVER, .payload = buffer});
// }


// void Tuya::send_datapoint_command_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, std::vector<uint8_t> data) {
//   std::vector<uint8_t> buffer;
//   buffer.push_back(datapoint_id);
//   buffer.push_back(static_cast<uint8_t>(datapoint_type));
//   buffer.push_back(data.size() >> 8);
//   buffer.push_back(data.size() >> 0);
//   buffer.insert(buffer.end(), data.begin(), data.end());

//   this->send_command_(TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_DELIVER, .payload = buffer});
// }

void Tuya::send_datapoint_command_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, std::vector<uint8_t> data) {
  // NUCLEAR: Ignore init_state, ignore "ready" checks.
  // If we have data, we send it.

  uint16_t length = data.size() + 2;
  std::vector<uint8_t> buffer;
  buffer.push_back(datapoint_id);
  buffer.push_back(static_cast<uint8_t>(datapoint_type));
  buffer.push_back(length >> 8);
  buffer.push_back(length & 0xFF);
  buffer.insert(buffer.end(), data.begin(), data.end());

  ESP_LOGD(TAG, "NUCLEAR: Forcing Command Out -> DP: %u", datapoint_id);

  // Directly call the low-level send without checking the queue state
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_DELIVER, .payload = buffer});
}

void Tuya::register_listener(uint8_t datapoint_id, const std::function<void(TuyaDatapoint)> &func) {
  auto listener = TuyaDatapointListener{
      .datapoint_id = datapoint_id,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  // Run through existing datapoints
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      func(datapoint);
  }
}

void Tuya::process_record_report_(const uint8_t *buffer, size_t len) {
  // Detection: Historical packets prefix data with time.
  // 7 bytes = YY MM DD HH MM SS + Sub-second/Type
  // 13 bytes = Unix Millisecond Epoch (found on newer Zigbee/Bluetooth gateways)
  size_t offset = (len > 13) ? 13 : 7;

  if (len <= offset) {
    ESP_LOGW(TAG, "Historical packet (0x08) too short to contain timestamp (len: %zu)", len);
    return;
  }

  size_t data_len = len - offset;

  // Logic Safety: A Tuya DP needs at least 4 bytes (ID, Type, Len_Hi, Len_Lo)
  if (data_len < 4) {
    ESP_LOGW(TAG, "Short historical packet: After stripping %zu-byte timestamp, only %zu bytes remain (need 4+)",
             offset, data_len);
    // Still send the ACK so the MCU doesn't loop forever
    this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
    return;
  }

  ESP_LOGD(TAG, "Stripping %zu-byte timestamp. Parsing %zu bytes of historical sensor data.", offset, data_len);

  // Pass the 'cleaned' buffer to the standard parser
  this->handle_datapoints_(buffer + offset, data_len);

  // Mandatory ACK for 0x08 to allow MCU to sleep
  this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
}

TuyaInitState Tuya::get_init_state() { return this->init_state_; }

}  // namespace tuya
}  // namespace esphome
