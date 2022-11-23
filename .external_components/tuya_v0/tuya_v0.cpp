#include "tuya_v0.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tuya_v0 {

static const char *TAG = "tuya";
static const int COMMAND_DELAY = 50;

void Tuya::setup() {
  this->set_interval("heartbeat", 10000, [this] { this->send_empty_command_(TuyaCommandType::HEARTBEAT); });
}

void Tuya::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }
  process_command_queue_();
}

void Tuya::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya:");
  if (this->init_state_ != TuyaInitState::INIT_DONE) {
    ESP_LOGCONFIG(TAG, "  Configuration will be reported when setup is complete. Current init_state: %u",
                  static_cast<uint8_t>(this->init_state_));
    ESP_LOGCONFIG(TAG, "  If no further output is received, confirm that this is a supported Tuya device.");
    return;
  }
  for (auto &info : this->datapoints_) {
    if (info.type == TuyaDatapointType::BOOLEAN)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: switch (value: %s)", info.id, ONOFF(info.value_bool));
    else if (info.type == TuyaDatapointType::INTEGER)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: int value (value: %d)", info.id, info.value_int);
    else if (info.type == TuyaDatapointType::STRING)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: string value (value: %s)", info.id, info.value_string.c_str());
    else if (info.type == TuyaDatapointType::ENUM)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: enum (value: %d)", info.id, info.value_enum);
    else if (info.type == TuyaDatapointType::BITMASK)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: bitmask (value: %x)", info.id, info.value_bitmask);
    else
      ESP_LOGCONFIG(TAG, "  Datapoint %d: unknown", info.id);
  }
  if ((this->gpio_status_ != -1) || (this->gpio_reset_ != -1)) {
    ESP_LOGCONFIG(TAG, "  GPIO Configuration: status: pin %d, reset: pin %d (not supported)", this->gpio_status_,
                  this->gpio_reset_);
  }
  ESP_LOGCONFIG(TAG, "  Product: '%s'", this->product_.c_str());
  this->check_uart_settings(9600);
}

bool Tuya::validate_message_() {
  uint32_t at = this->rx_message_.size() - 1;
  auto *data = &this->rx_message_[0];
  uint8_t new_byte = data[at];

  // Byte 0: HEADER1 (always 0x55)
  if (at == 0)
    return new_byte == 0x55;
  // Byte 1: HEADER2 (always 0xAA)
  if (at == 1)
    return new_byte == 0xAA;

  // Byte 2: COMMAND
  uint8_t command = data[2];
  if (at == 2)
    return true;

  // Byte 3: LENGTH
  if (at == 3)
    // no validation for these fields
    return true;

  uint8_t length = data[3];

  // wait until all data is read
  if (at - 3 < length)
    return true;

  if (at == 3 + length) {
    // Byte 6+LEN: CHECKSUM - sum of all bytes (including header) modulo 256
    uint8_t rx_checksum = new_byte;
    uint8_t calc_checksum = 0;
    for (uint32_t i = 2; i <= 2 + length; i++)
      calc_checksum += data[i];

    if (rx_checksum != calc_checksum) {
      ESP_LOGW(TAG, "Tuya Received invalid message checksum %02X!=%02X", rx_checksum, calc_checksum);
      return false;
    }
    return true;
  }

  if (at == 3 + length + 1 && new_byte != 0x7E) {
    ESP_LOGW(TAG, "Tuya Received invalid message tail (0x%02X)", new_byte);
    return false;
  }

  // valid message
  const uint8_t *message_data = data + 4;
  ESP_LOGV(TAG, "Received Tuya: CMD=0x%02X DATA=[%s] INIT_STATE=%u", command,  // NOLINT
           hexencode(message_data, length - 1).c_str(), this->init_state_);
  this->handle_command_(command, message_data, length - 1);

  // return false to reset rx buffer
  return false;
}

void Tuya::handle_char_(uint8_t c) {
  this->rx_message_.push_back(c);
  if (!this->validate_message_()) {
    this->rx_message_.clear();
  }
}

void Tuya::handle_command_(uint8_t command, const uint8_t *buffer, size_t len) {
  switch ((TuyaCommandType) command) {
    case TuyaCommandType::HEARTBEAT:
      ESP_LOGV(TAG, "MCU Heartbeat (0x%02X)", buffer[0]);
      if (buffer[0] == 0) {
        ESP_LOGI(TAG, "MCU restarted");
        this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
      }
      if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT) {
        this->init_state_ = TuyaInitState::INIT_PRODUCT;
        // this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
      }
      break;
    // case TuyaCommandType::PRODUCT_QUERY: {
    //   // check it is a valid string made up of printable characters
    //   bool valid = true;
    //   for (int i = 0; i < len; i++) {
    //     if (!std::isprint(buffer[i])) {
    //       valid = false;
    //       break;
    //     }
    //   }
    //   if (valid) {
    //     this->product_ = std::string(reinterpret_cast<const char *>(buffer), len);
    //   } else {
    //     this->product_ = R"({"p":"INVALID"})";
    //   }
    //   if (this->init_state_ == TuyaInitState::INIT_PRODUCT) {
    //     this->init_state_ = TuyaInitState::INIT_CONF;
    //     this->send_empty_command_(TuyaCommandType::CONF_QUERY);
    //   }
    //   break;
    // }
    // case TuyaCommandType::CONF_QUERY: {
    //   if (len >= 2) {
    //     gpio_status_ = buffer[0];
    //     gpio_reset_ = buffer[1];
    //   }
    //   if (this->init_state_ == TuyaInitState::INIT_CONF) {
    //     // If mcu returned status gpio, then we can ommit sending wifi state
    //     if (this->gpio_status_ != -1) {
    //       this->init_state_ = TuyaInitState::INIT_DATAPOINT;
    //       this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
    //     } else {
    //       this->init_state_ = TuyaInitState::INIT_WIFI;
    //       // If we were following the spec to the letter we would send
    //       // state updates until connected to both WiFi and API/MQTT.
    //       // Instead we just claim to be connected immediately and move on.
    //       this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WIFI_STATE, .payload =
    //       std::vector<uint8_t>{0x04}});
    //     }
    //   }
    //   break;
    // }
    // case TuyaCommandType::WIFI_STATE:
    //   if (this->init_state_ == TuyaInitState::INIT_WIFI) {
    //     this->init_state_ = TuyaInitState::INIT_DATAPOINT;
    //     this->send_empty_command_(TuyaCommandType::DATAPOINT_QUERY);
    //   }
    //   break;
    case TuyaCommandType::WIFI_RESET:
      ESP_LOGE(TAG, "TUYA_CMD_WIFI_RESET is not handled");
      break;
    // case TuyaCommandType::WIFI_SELECT:
    //   ESP_LOGE(TAG, "TUYA_CMD_WIFI_SELECT is not handled");
    //   break;
    // case TuyaCommandType::DATAPOINT_DELIVER:
    //   break;
    // case TuyaCommandType::DATAPOINT_REPORT:
    //   if (this->init_state_ == TuyaInitState::INIT_DATAPOINT) {
    //     this->init_state_ = TuyaInitState::INIT_DONE;
    //     this->set_timeout("datapoint_dump", 1000, [this] { this->dump_config(); });
    //   }
    //   this->handle_datapoint_(buffer, len);
    //   break;
    // case TuyaCommandType::DATAPOINT_QUERY:
    //   break;
    // case TuyaCommandType::WIFI_TEST: {
    //   this->send_command_(TuyaCommand{.cmd = TuyaCommandType::WIFI_TEST, .payload = std::vector<uint8_t>{0x00,
    //   0x00}}); break;
    // }
    case TuyaCommandType::DATA_UPDATE: {
      this->handle_datapoint_(buffer, len);
      break;
    }
    default:
      ESP_LOGE(TAG, "invalid command (%02x) received", command);
  }
}

void Tuya::handle_datapoint_(const uint8_t *buffer, size_t len) {
  if (len < 2)
    return;

  for (auto &listener : this->listeners_) {
    TuyaDatapoint datapoint{};
    datapoint.id = listener.datapoint_id;
    datapoint.type = listener.type;
    datapoint.value_uint = 0;

    // drop update if datapoint is in ignore_mcu_datapoint_update list
    for (auto ignored : this->ignore_mcu_update_on_datapoints_) {
      if (datapoint.id == ignored) {
        ESP_LOGV(TAG, "Datapoint index %u found in ignore_mcu_update_on_datapoints list, dropping MCU update",
                 datapoint.id);
        return;
      }
    }

    switch (datapoint.type) {
      case TuyaDatapointType::BOOLEAN:
        datapoint.value_bool = buffer[datapoint.id] == 0x01;
        break;
      case TuyaDatapointType::INTEGER:
        datapoint.value_uint = buffer[datapoint.id];
        break;
      case TuyaDatapointType::ENUM:
        datapoint.value_enum = buffer[datapoint.id];
        break;
      default:
        return;
    }
    ESP_LOGV(TAG, "Datapoint %u update to %u", datapoint.id, datapoint.value_uint);

    // Update internal datapoints
    bool found = false;
    for (auto &other : this->datapoints_) {
      if (other.id == datapoint.id) {
        other = datapoint;
        found = true;
      }
    }
    if (!found) {
      this->datapoints_.push_back(datapoint);
    }

    listener.on_datapoint(datapoint);
  }
}

void Tuya::send_raw_command_(TuyaCommand command) {
  uint8_t length = (uint8_t)(command.payload.size());

  this->last_command_timestamp_ = millis();

  ESP_LOGV(TAG, "Sending Tuya: CMD=0x%02X DATA=[%s] INIT_STATE=%u", command.cmd,  // NOLINT
           hexencode(command.payload).c_str(), this->init_state_);

  this->write_array({0x55, 0xAA, (uint8_t) command.cmd});
  if (!command.payload.empty())
    this->write_array(command.payload.data(), command.payload.size());

  uint8_t checksum = (uint8_t) command.cmd;
  for (auto &data : command.payload)
    checksum += data;
  this->write_byte(checksum);
  this->write_byte(0x7E);
}

void Tuya::process_command_queue_() {
  uint32_t delay = millis() - this->last_command_timestamp_;
  // Left check of delay since last command in case theres ever a command sent by calling send_raw_command_ directly
  if (delay > COMMAND_DELAY && !command_queue_.empty()) {
    this->send_raw_command_(command_queue_.front());
    this->command_queue_.erase(command_queue_.begin());
  }
}

void Tuya::send_command_(TuyaCommand command) {
  command_queue_.push_back(command);
  process_command_queue_();
}

void Tuya::send_empty_command_(TuyaCommandType command) {
  send_command_(TuyaCommand{.cmd = command, .payload = std::vector<uint8_t>{0x00}});
}

void Tuya::set_datapoint_value(std::vector<uint8_t> &data) {
  data.insert(data.begin(), data.size() + 1);
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::DATA_UPDATE, .payload = data});
}

void Tuya::register_listener(uint8_t datapoint_id, TuyaDatapointType type,
                             const std::function<void(TuyaDatapoint)> &func) {
  auto listener = TuyaDatapointListener{
      .datapoint_id = datapoint_id,
      .type = type,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  // Run through existing datapoints
  for (auto &datapoint : this->datapoints_)
    if (datapoint.id == datapoint_id)
      func(datapoint);
}

}  // namespace tuya_v0
}  // namespace esphome
