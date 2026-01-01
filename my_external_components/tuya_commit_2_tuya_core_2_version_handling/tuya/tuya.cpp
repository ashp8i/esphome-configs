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
static const int COMMAND_DELAY = 20;  // Increased delay
static const int RECEIVE_TIMEOUT = 300;
static const int MAX_RETRIES = 5;

void Tuya::setup() {
  this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
  // Kickstart the first one immediately
  this->send_empty_command_(TuyaCommandType::HEARTBEAT);

  // Keep the long-term interval
  this->set_interval("heartbeat", 15000, [this] {
    this->send_empty_command_(TuyaCommandType::HEARTBEAT);
  });
}

void Tuya::loop() {
  // Process input buffer first
  this->process_input_buffer_();

  // Process command queue
  this->process_command_queue_();
}

void Tuya::process_input_buffer_() {
  // Read available data with limits to prevent buffer bloat
  static uint32_t last_read_time = 0;
  uint32_t now = millis();

  // Rate limit reading to prevent overwhelming the loop
  if (now - last_read_time < 10) {
    return;
  }
  last_read_time = now;

  // Read in small chunks to prevent blocking
  uint8_t buffer[64];
  int bytes_available = this->available();

  // Limit read size to prevent memory issues
  int read_size = std::min(bytes_available, 64);

  if (read_size > 0) {
    this->read_array(buffer, read_size);

    // Add to our receiving buffer
    for (int i = 0; i < read_size; i++) {
      this->rx_message_.push_back(buffer[i]);
    }
  }

  // Process frames if we have enough data
  this->process_frames_();
}

void Tuya::process_frames_() {
  // Process frames with safety limits
  int processed_frames = 0;
  const int MAX_FRAMES_PER_LOOP = 5;  // Prevent infinite loops

  while (processed_frames < MAX_FRAMES_PER_LOOP && this->rx_message_.size() >= 7) {
    // Look for Header: 0x55 0xAA
    if (this->rx_message_[0] != 0x55 || this->rx_message_[1] != 0xAA) {
      this->rx_message_.pop_front();
      continue;
    }

    uint8_t version = this->rx_message_[2];
    uint8_t command = this->rx_message_[3];
    uint16_t length = (uint16_t(this->rx_message_[4]) << 8) | this->rx_message_[5];
    size_t total_len = length + 7;

    // Safety check for excessive length
    if (length > 1024) {
      ESP_LOGW(TAG, "Received excessive packet length: %u", length);
      this->rx_message_.clear();
      return;
    }

    // Is the full frame in the buffer?
    if (this->rx_message_.size() < total_len) {
      break;
    }

    // Verify Checksum
    uint8_t checksum = 0;
    for (size_t i = 0; i < total_len - 1; i++) {
      checksum += this->rx_message_[i];
    }

    if (checksum != this->rx_message_[total_len - 1]) {
      ESP_LOGW(TAG, "Tuya Checksum Fail! Expected 0x%02X", checksum);
      this->rx_message_.pop_front(); // Slide past the bad header
      continue;
    }

    // Prepare the payload pointer
    const uint8_t *payload_ptr = &this->rx_message_[6];

    // Process the command
    this->handle_command_(command, version, payload_ptr, length);

    // Clean up the processed frame
    this->rx_message_.erase(this->rx_message_.begin(), this->rx_message_.begin() + total_len);

    processed_frames++;
  }
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

void Tuya::handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
  TuyaCommandType command_type = (TuyaCommandType) command;

  // VIP GATE: Immediate Datapoint Processing (0x07, 0x08, 0x05)
  if (command == 0x07 || command == 0x08 || command == 0x05) {
    this->handle_datapoints_(buffer, len);

    // Sync Report ACK (0x09)
    if (command == 0x08) {
      this->send_command_(TuyaCommand{static_cast<TuyaCommandType>(0x09), {0x01}});
    }
    return; // Exit early; don't touch the state machine switch
  }

  switch (command_type) {
    case TuyaCommandType::HEARTBEAT:
      this->send_empty_command_(TuyaCommandType::HEARTBEAT);
      if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT) {
        this->init_state_ = TuyaInitState::INIT_PRODUCT;
        this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
      }
      break;

    case TuyaCommandType::PRODUCT_QUERY:
      this->product_ = std::string(reinterpret_cast<const char *>(buffer), len);
      if (this->init_state_ == TuyaInitState::INIT_PRODUCT) {
        this->init_state_ = TuyaInitState::INIT_CONF;
        this->send_empty_command_(TuyaCommandType::CONF_QUERY);
      }
      break;

    case TuyaCommandType::CONF_QUERY:
      this->send_command_(TuyaCommand{TuyaCommandType::GET_NETWORK_STATUS, {this->get_wifi_status_code_()}});
      break;

    case TuyaCommandType::GET_NETWORK_STATUS:
      this->send_command_(TuyaCommand{TuyaCommandType::GET_NETWORK_STATUS, {this->get_wifi_status_code_()}});
      if (this->init_state_ != TuyaInitState::INIT_DONE) {
        this->init_state_ = TuyaInitState::INIT_DONE;
        ESP_LOGI(TAG, "Flood-Proof Handshake Complete.");
      }
      break;

    case TuyaCommandType::LOCAL_TIME_QUERY:
#ifdef USE_TIME
      if (this->time_id_ != nullptr) {
        // Only respond if we actually have a valid time to give
        if (this->time_id_->now().is_valid()) {
           this->send_local_time_();
        } else {
           // MCU is spamming, but we have no time.
           // We stay silent to keep the serial line clear for the Heartbeat.
           ESP_LOGV(TAG, "MCU requested time, but ESPTime is not valid yet. Ignoring to prevent flood.");
        }

        if (!this->time_sync_callback_registered_) {
          this->time_id_->add_on_time_sync_callback([this] { this->send_local_time_(); });
          this->time_sync_callback_registered_ = true;
        }
      }
#endif
      break;
    case TuyaCommandType::VACUUM_MAP_UPLOAD:
      // Map ACK (0x63)
      this->send_command_(TuyaCommand{static_cast<TuyaCommandType>(0x63), {0x01}});
      break;

    default:
      // Silently ignore to prevent log-induced DOS
      break;
  }

  // Handle Command Queue/ACK clearing
  if (this->expected_response_.has_value() && command_type == *this->expected_response_) {
    this->expected_response_.reset();
    if (!this->command_queue_.empty()) this->command_queue_.pop_front();
  }
}

void Tuya::handle_datapoints_(const uint8_t *buffer, size_t len) {
  // Safety limit to prevent infinite loops
  int processed_datapoints = 0;
  const int MAX_DATPOINTS_PER_CALL = 10;

  while (len >= 4 && processed_datapoints < MAX_DATPOINTS_PER_CALL) {
    TuyaDatapoint datapoint{};
    datapoint.id = buffer[0];
    datapoint.type = (TuyaDatapointType) buffer[1];
    datapoint.value_uint = 0;

    size_t data_size = (uint16_t(buffer[2]) << 8) | buffer[3];
    const uint8_t *data = buffer + 4;

    // Safety check for excessive data size
    if (data_size > 1024) {
      ESP_LOGW(TAG, "Datapoint %u has excessive data size: %zu", datapoint.id, data_size);
      return;
    }

    // 1. Structural Safety Check
    if (data_size + 4 > len) {
      ESP_LOGW(TAG, "Datapoint %u truncated! Expected %zu, got %zu", datapoint.id, data_size + 4, len);
      return;
    }
    datapoint.len = data_size;

    // 2. Early Ignore Check (Saves CPU cycles during storms)
    bool skip = false;
    for (uint8_t ignored_id : this->ignore_mcu_update_on_datapoints_) {
      if (datapoint.id == ignored_id) { skip = true; break; }
    }

    // 3. Type-Strict Parsing
    switch (datapoint.type) {
      case TuyaDatapointType::RAW:
        datapoint.value_raw.assign(data, data + data_size);
        break;

      case TuyaDatapointType::BOOLEAN:
        if (data_size != 1) { ESP_LOGW(TAG, "DP %u bad bool len", datapoint.id); return; }
        datapoint.value_bool = data[0];
        if (!skip) ESP_LOGD(TAG, "DP %u (Bool): %s", datapoint.id, ONOFF(datapoint.value_bool));
        break;

      case TuyaDatapointType::INTEGER:
        if (data_size != 4) { ESP_LOGW(TAG, "DP %u bad int len", datapoint.id); return; }
        datapoint.value_uint = encode_uint32(data[0], data[1], data[2], data[3]);
        if (!skip) ESP_LOGD(TAG, "DP %u (Int): %u", datapoint.id, datapoint.value_uint);
        break;

      case TuyaDatapointType::STRING:
        datapoint.value_string = std::string(reinterpret_cast<const char *>(data), data_size);
        if (!skip) ESP_LOGD(TAG, "DP %u (String): %s", datapoint.id, datapoint.value_string.c_str());
        break;

      case TuyaDatapointType::ENUM:
        if (data_size != 1) { ESP_LOGW(TAG, "DP %u bad enum len", datapoint.id); return; }
        datapoint.value_enum = data[0];
        if (!skip) ESP_LOGD(TAG, "DP %u (Enum): %u", datapoint.id, datapoint.value_enum);
        break;

      case TuyaDatapointType::BITMASK:
        if (data_size == 1) datapoint.value_bitmask = encode_uint32(0, 0, 0, data[0]);
        else if (data_size == 2) datapoint.value_bitmask = encode_uint32(0, 0, data[0], data[1]);
        else if (data_size == 4) datapoint.value_bitmask = encode_uint32(data[0], data[1], data[2], data[3]);
        else { ESP_LOGW(TAG, "DP %u bad bitmask len", datapoint.id); return; }
        if (!skip) ESP_LOGD(TAG, "DP %u (Bitmask): %08X", datapoint.id, datapoint.value_bitmask);
        break;

      default:
        ESP_LOGW(TAG, "DP %u unknown type 0x%02X", datapoint.id, (uint8_t)datapoint.type);
        return;
    }

    // 4. Update Internal State & Listeners
    if (!skip) {
      auto it = std::find_if(this->datapoints_.begin(), this->datapoints_.end(),
                             [&](const TuyaDatapoint &d) { return d.id == datapoint.id; });
      if (it != this->datapoints_.end()) {
        *it = datapoint;
      } else {
        this->datapoints_.push_back(datapoint);
      }

      for (auto &listener : this->listeners_) {
        if (listener.datapoint_id == datapoint.id) {
          listener.on_datapoint(datapoint);
        }
      }
    }

    // 5. Advance Buffer
    len -= (data_size + 4);
    buffer += (data_size + 4);
    processed_datapoints++;
  }
}

void Tuya::send_raw_command_(TuyaCommand command) {
  // Safety check for excessive command size
  if (command.payload.size() > 1024) {
    ESP_LOGE(TAG, "Command payload too large: %zu bytes", command.payload.size());
    return;
  }

  uint8_t len_hi = (uint8_t) (command.payload.size() >> 8);
  uint8_t len_lo = (uint8_t) (command.payload.size() & 0xFF);
  uint8_t version = (this->protocol_version_ == 0xFF) ? 0x00 : this->protocol_version_;

  this->last_command_timestamp_ = millis();

  switch (command.cmd) {
    case TuyaCommandType::HEARTBEAT: this->expected_response_ = TuyaCommandType::HEARTBEAT; break;
    case TuyaCommandType::PRODUCT_QUERY: this->expected_response_ = TuyaCommandType::PRODUCT_QUERY; break;
    case TuyaCommandType::CONF_QUERY: this->expected_response_ = TuyaCommandType::CONF_QUERY; break;
    case TuyaCommandType::DATAPOINT_DELIVER:
    case TuyaCommandType::DATAPOINT_QUERY: this->expected_response_ = TuyaCommandType::DATAPOINT_REPORT_ASYNC; break;
    default: break;
  }

  ESP_LOGV(TAG, "TX: CMD=0x%02X VER=%u LEN=%d", static_cast<uint8_t>(command.cmd), version, command.payload.size());

  this->write_array({0x55, 0xAA, version, (uint8_t) command.cmd, len_hi, len_lo});
  if (!command.payload.empty()) {
    this->write_array(command.payload.data(), command.payload.size());
  }

  uint8_t checksum = 0x55 + 0xAA + version + (uint8_t) command.cmd + len_hi + len_lo;
  for (auto &data : command.payload) {
    checksum += data;
  }
  this->write_byte(checksum);
}

void Tuya::process_command_queue_() {
  uint32_t now = millis();
  static uint32_t last_process_time = 0;

  // Rate limit processing to prevent busy loops
  if (now - last_process_time < 5) {
    return;
  }
  last_process_time = now;

  // Process input buffer first
  while (this->validate_message_()) {
    this->last_rx_char_timestamp_ = now;
  }

  if (this->expected_response_.has_value()) {
    if (now - this->last_command_timestamp_ > 2000) {  // Increased timeout
      ESP_LOGW(TAG, "Timed out waiting for 0x%02X", (uint8_t)*this->expected_response_);
      this->expected_response_.reset();
    }
  }

  // Process Output Queue
  if (!this->command_queue_.empty()) {
    // Respect the delay (politeness)
    if (now - this->last_command_timestamp_ > COMMAND_DELAY) {
      this->send_raw_command_(this->command_queue_.front());
      this->command_queue_.pop_front(); // Pop AFTER sending
    }
  }
}

void Tuya::send_command_(const TuyaCommand &command) {
  if (command.payload.size() > 1024) {
    ESP_LOGE(TAG, "Command too large to queue: %zu bytes", command.payload.size());
    return;
  }
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
  uint8_t status = 0x02; // Connecting
  if (network::is_connected()) {
    status = 0x03; // Connected to Router
    if (this->protocol_version_ >= 0x03 && remote_is_connected()) {
      status = 0x04; // Connected to Cloud
    }
  } else {
#ifdef USE_CAPTIVE_PORTAL
    if (captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active()) {
      status = 0x01; // SmartConfig / AP Mode
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
  ESP_LOGD(TAG, "Sending WiFi Status: %02X", status);
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

void Tuya::set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint32_t value,
                                        uint8_t length, bool forced) {
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
    // For manual transmission, we allow sending even if unknown
  } else if (datapoint->type != datapoint_type) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_uint == value) {
    return;
  }

  std::vector<uint8_t> data;
  switch (length) {
    case 4:
      data.push_back(value >> 24);
      data.push_back(value >> 16);
    case 2:
      data.push_back(value >> 8);
    case 1:
      data.push_back(value >> 0);
      break;
    default:
      ESP_LOGE(TAG, "Unexpected datapoint length %u", length);
      return;
  }
  this->send_datapoint_command_(datapoint_id, datapoint_type, data);
}

void Tuya::set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced) {
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
  } else if (datapoint->type != TuyaDatapointType::RAW) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_raw == value) {
    return;
  }
  this->send_datapoint_command_(datapoint_id, TuyaDatapointType::RAW, value);
}

void Tuya::set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced) {
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
  } else if (datapoint->type != TuyaDatapointType::STRING) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_string == value) {
    return;
  }
  std::vector<uint8_t> data(value.begin(), value.end());
  this->send_datapoint_command_(datapoint_id, TuyaDatapointType::STRING, data);
}

void Tuya::send_datapoint_command_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, std::vector<uint8_t> data) {
  std::vector<uint8_t> buffer;
  buffer.push_back(datapoint_id);
  buffer.push_back(static_cast<uint8_t>(datapoint_type));
  buffer.push_back(data.size() >> 8);
  buffer.push_back(data.size() >> 0);
  buffer.insert(buffer.end(), data.begin(), data.end());

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

TuyaInitState Tuya::get_init_state() { return this->init_state_; }

}  // namespace tuya
}  // namespace esphome
