#include "tuya.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/util.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya";

void Tuya::setup() {
  this->set_interval("heartbeat", 1000, [this] { this->send_empty_command_(TuyaCommandTypeV0::HEARTBEAT); });
}

void Tuya::loop() {
  if (this->version_ == 0) {
    WifiState current_state = WifiState::DISCONNECTED;
    if (network_is_connected())
      current_state = WifiState::CONNECTED;
#ifdef USE_MQTT
    if ((mqtt::global_mqtt_client != nullptr) && mqtt::global_mqtt_client->is_connected())
      current_state = WifiState::CONNECTED_DATA;
#endif
#ifdef USE_API
    if ((api::global_api_server != nullptr) && api::global_api_server->is_connected())
      current_state = WifiState::CONNECTED_DATA;
#endif
    if (current_state != this->wifi_state_) {
      this->wifi_state_ = current_state;
      uint8_t c = (uint8_t) current_state;
      this->send_command_(TuyaCommandTypeV0::NETWORK_STATUS, &c, 1);
      ESP_LOGV(TAG, "setting wifi state to %u", c);
    }
  }
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }
}

void Tuya::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya:");
  if (this->init_state_ != TuyaInitState::INIT_DONE) {
    ESP_LOGCONFIG(TAG, "  Configuration will be reported when setup is complete. Current init_state: %u",
                  static_cast<uint8_t>(this->init_state_));
    ESP_LOGCONFIG(TAG, "  If no further output is received, confirm that this is a supported Tuya device.");
    return;
  }
  if (this->version_ == 0)
    ESP_LOGCONFIG(TAG, "  Battery powered device detected");
  for (auto &info : this->datapoints_) {
    if (info.type == TuyaDatapointType::BOOLEAN)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: switch (value: %s)", info.id, ONOFF(info.value_bool));
    else if (info.type == TuyaDatapointType::INTEGER)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: int value (value: %d)", info.id, info.value_int);
    else if (info.type == TuyaDatapointType::ENUM)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: enum (value: %d)", info.id, info.value_enum);
    else if (info.type == TuyaDatapointType::BITMASK)
      ESP_LOGCONFIG(TAG, "  Datapoint %d: bitmask (value: %x)", info.id, info.value_bitmask);
    else
      ESP_LOGCONFIG(TAG, "  Datapoint %d: unknown type (%x)", info.id, (int) info.type);
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

  // Byte 2: VERSION
  // no validation for the following fields:
  uint8_t version = data[2];
  if (at == 2)
    return true;
  // Byte 3: COMMAND
  uint8_t command = data[3];
  if (at == 3)
    return true;

  // Byte 4: LENGTH1
  // Byte 5: LENGTH2
  if (at <= 5)
    // no validation for these fields
    return true;

  uint16_t length = (uint16_t(data[4]) << 8) | (uint16_t(data[5]));

  // wait until all data is read
  if (at - 6 < length)
    return true;

  // Byte 6+LEN: CHECKSUM - sum of all bytes (including header) modulo 256
  uint8_t rx_checksum = new_byte;
  uint8_t calc_checksum = 0;
  for (uint32_t i = 0; i < 6 + length; i++)
    calc_checksum += data[i];

  if (rx_checksum != calc_checksum) {
    ESP_LOGW(TAG, "Tuya Received invalid message checksum %02X!=%02X", rx_checksum, calc_checksum);
    return false;
  }

  // valid message
  const uint8_t *message_data = data + 6;
  ESP_LOGV(TAG, "Received Tuya: CMD=0x%02X VERSION=%u DATA=[%s] INIT_STATE=%u", command, version,
           hexencode(message_data, length).c_str(), this->init_state_);
  if (this->version_ == 0)
    this->handle_command_((TuyaCommandTypeV0) command, version, message_data, length);
  else
    this->handle_command_((TuyaCommandTypeV3) command, version, message_data, length);

  // return false to reset rx buffer
  return false;
}

void Tuya::handle_char_(uint8_t c) {
  this->rx_message_.push_back(c);
  if (!this->validate_message_()) {
    this->rx_message_.clear();
  }
}

void Tuya::handle_command_(TuyaCommandTypeV0 command, uint8_t version, const uint8_t *buffer, size_t len) {
  switch (command) {
    case TuyaCommandTypeV0::WIFI_RESET:
      this->send_empty_command_(TuyaCommandTypeV0::WIFI_RESET);
      break;
    case TuyaCommandTypeV0::WIFI_CONF:
      ESP_LOGV(TAG, "MCU requested %s wifi mode", buffer[0] == 0 ? "smartconfig" : "AP");
      this->send_empty_command_(TuyaCommandTypeV0::WIFI_CONF);
      break;
    case TuyaCommandTypeV0::REALTIME_REPORT:
      this->set_timeout("datapoint_dump", 1000, [this] { this->dump_config(); });
      this->handle_datapoints_(buffer, len);
      break;
    default:
      break;
  }
}

void Tuya::handle_command_(TuyaCommandTypeV3 command, uint8_t version, const uint8_t *buffer, size_t len) {
  switch (command) {
    case TuyaCommandTypeV3::HEARTBEAT:
      ESP_LOGV(TAG, "MCU Heartbeat (0x%02X)", buffer[0]);
      if (buffer[0] == 0) {
        ESP_LOGI(TAG, "MCU restarted");
        this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
      }
      if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT) {
        this->init_state_ = TuyaInitState::INIT_PRODUCT;
        this->send_empty_command_(TuyaCommandTypeV3::PRODUCT_QUERY);
      }
      break;
    case TuyaCommandTypeV3::PRODUCT_QUERY: {
      // check it is a valid string made up of printable characters
      bool valid = true;
      for (int i = 0; i < len; i++) {
        if (!std::isprint(buffer[i])) {
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
        this->version_ = version;
        if (version == 0) {
          this->init_state_ = TuyaInitState::INIT_DONE;
          uint8_t c = 1;
          this->send_command_(TuyaCommandTypeV0::NETWORK_STATUS, &c, 1);
        } else {
          this->init_state_ = TuyaInitState::INIT_CONF;
          this->send_empty_command_(TuyaCommandTypeV3::CONF_QUERY);
        }
      }
      break;
    }
    case TuyaCommandTypeV3::CONF_QUERY: {
      if (len >= 2) {
        gpio_status_ = buffer[0];
        gpio_reset_ = buffer[1];
      }
      if (this->init_state_ == TuyaInitState::INIT_CONF) {
        // If we were following the spec to the letter we would send
        // state updates until connected to both WiFi and API/MQTT.
        // Instead we just claim to be connected immediately and move on.
        uint8_t c[] = {0x04};
        this->init_state_ = TuyaInitState::INIT_WIFI;
        this->send_command_(TuyaCommandTypeV3::WIFI_STATE, c, 1);
      }
      break;
    }
    case TuyaCommandTypeV3::WIFI_STATE:
      if (this->init_state_ == TuyaInitState::INIT_WIFI) {
        this->init_state_ = TuyaInitState::INIT_DATAPOINT;
        this->send_empty_command_(TuyaCommandTypeV3::DATAPOINT_QUERY);
      }
      break;
    case TuyaCommandTypeV3::WIFI_RESET:
      ESP_LOGD(TAG, "MCU requested Wifi reset");
      this->send_empty_command_(TuyaCommandTypeV3::WIFI_RESET);
      break;
    case TuyaCommandTypeV3::WIFI_SELECT:
      ESP_LOGD(TAG, "MCU requested %s Wifi mode", buffer[0] == 0 ? "smartconfig" : "AP");
      this->send_empty_command_(TuyaCommandTypeV3::WIFI_SELECT);
      break;
    case TuyaCommandTypeV3::DATAPOINT_DELIVER:
      break;
    case TuyaCommandTypeV3::DATAPOINT_REPORT:
      if (this->init_state_ == TuyaInitState::INIT_DATAPOINT) {
        this->init_state_ = TuyaInitState::INIT_DONE;
        this->set_timeout("datapoint_dump", 1000, [this] { this->dump_config(); });
      }
      this->handle_datapoints_(buffer, len);
      break;
    case TuyaCommandTypeV3::DATAPOINT_QUERY:
      break;
    case TuyaCommandTypeV3::WIFI_TEST: {
      uint8_t c[] = {0x00, 0x00};
      this->send_command_(TuyaCommandTypeV3::WIFI_TEST, c, 2);
      break;
    }
    default:
      ESP_LOGE(TAG, "invalid command (%02x) received", command);
  }
}

void Tuya::handle_datapoints_(const uint8_t *buffer, size_t len) {
  while (len >= 4) {
    TuyaDatapoint datapoint{};
    datapoint.id = buffer[0];
    datapoint.type = (TuyaDatapointType) buffer[1];
    datapoint.value_uint = 0;

    size_t data_size = (buffer[2] << 8) + buffer[3];
    const uint8_t *data = buffer + 4;
    if (data_size > len - 4) {
      ESP_LOGW(TAG, "invalid datapoint update");
      return;
    }

    switch (datapoint.type) {
      case TuyaDatapointType::BOOLEAN:
        if (data_size != 1)
          break;
        datapoint.value_bool = data[0];
        break;
      case TuyaDatapointType::INTEGER:
        if (data_size != 4)
          break;
        datapoint.value_uint =
            (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) | (uint32_t(data[2]) << 8) | (uint32_t(data[3]) << 0);
        break;
      case TuyaDatapointType::ENUM:
        if (data_size != 1)
          break;
        datapoint.value_enum = data[0];
        break;
      case TuyaDatapointType::BITMASK:
        if (data_size != 2)
          break;
        datapoint.value_bitmask = (uint16_t(data[0]) << 8) | (uint16_t(data[1]) << 0);
        break;
      default:
        ESP_LOGW(TAG, "unknown datapoint type %u (0x%x)", datapoint.type, datapoint.type);
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

    // Run through listeners
    for (auto &listener : this->listeners_)
      if (listener.datapoint_id == datapoint.id)
        listener.on_datapoint(datapoint);

    len -= data_size + 4;
    buffer = data + data_size;
  }
}

void Tuya::send_command_(uint8_t command, const uint8_t *buffer, uint16_t len) {
  uint8_t len_hi = len >> 8;
  uint8_t len_lo = len >> 0;
  uint8_t version = 0;

  ESP_LOGV(TAG, "Sending Tuya: CMD=0x%02X VERSION=%u DATA=[%s] INIT_STATE=%u", command, version,
           hexencode(buffer, len).c_str(), this->init_state_);

  this->write_array({0x55, 0xAA, version, command, len_hi, len_lo});
  if (len != 0)
    this->write_array(buffer, len);

  uint8_t checksum = 0x55 + 0xAA + command + len_hi + len_lo;
  for (int i = 0; i < len; i++)
    checksum += buffer[i];
  this->write_byte(checksum);
}

void Tuya::set_datapoint_value(TuyaDatapoint datapoint) {
  std::vector<uint8_t> buffer;
  ESP_LOGV(TAG, "Datapoint %u set to %u", datapoint.id, datapoint.value_uint);
  for (auto &other : this->datapoints_) {
    if (other.id == datapoint.id) {
      if (other.value_uint == datapoint.value_uint) {
        ESP_LOGV(TAG, "Not sending unchanged value");
        return;
      }
    }
  }
  buffer.push_back(datapoint.id);
  buffer.push_back(static_cast<uint8_t>(datapoint.type));

  std::vector<uint8_t> data;
  switch (datapoint.type) {
    case TuyaDatapointType::BOOLEAN:
      data.push_back(datapoint.value_bool);
      break;
    case TuyaDatapointType::INTEGER:
      data.push_back(datapoint.value_uint >> 24);
      data.push_back(datapoint.value_uint >> 16);
      data.push_back(datapoint.value_uint >> 8);
      data.push_back(datapoint.value_uint >> 0);
      break;
    case TuyaDatapointType::ENUM:
      data.push_back(datapoint.value_enum);
      break;
    case TuyaDatapointType::BITMASK:
      data.push_back(datapoint.value_bitmask >> 8);
      data.push_back(datapoint.value_bitmask >> 0);
      break;
    default:
      return;
  }

  buffer.push_back(data.size() >> 8);
  buffer.push_back(data.size() >> 0);
  buffer.insert(buffer.end(), data.begin(), data.end());
  this->send_command_(TuyaCommandTypeV3::DATAPOINT_DELIVER, buffer.data(), buffer.size());
}

void Tuya::register_listener(uint8_t datapoint_id, const std::function<void(TuyaDatapoint)> &func) {
  auto listener = TuyaDatapointListener{
      .datapoint_id = datapoint_id,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  // Run through existing datapoints
  for (auto &datapoint : this->datapoints_)
    if (datapoint.id == datapoint_id)
      func(datapoint);
}

}  // namespace tuya
}  // namespace esphome
