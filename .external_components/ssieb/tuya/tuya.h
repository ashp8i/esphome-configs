#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace tuya {

enum class TuyaDatapointType : uint8_t {
  RAW = 0x00,      // variable length
  BOOLEAN = 0x01,  // 1 byte (0/1)
  INTEGER = 0x02,  // 4 byte
  STRING = 0x03,   // variable length
  ENUM = 0x04,     // 1 byte
  BITMASK = 0x05,  // 2 bytes
};

struct TuyaDatapoint {
  uint8_t id;
  TuyaDatapointType type;
  union {
    bool value_bool;
    int value_int;
    uint32_t value_uint;
    uint8_t value_enum;
    uint16_t value_bitmask;
  };
};

struct TuyaDatapointListener {
  uint8_t datapoint_id;
  std::function<void(TuyaDatapoint)> on_datapoint;
};

enum class WifiState : uint8_t {
  DISCONNECTED = 1,
  CONNECTED = 3,
  CONNECTED_DATA = 4,
};

enum class TuyaCommandTypeV0 : uint8_t {
  HEARTBEAT = 0x00,
  PRODUCT_QUERY = 0x01,
  NETWORK_STATUS = 0x02,
  WIFI_RESET = 0x03,
  WIFI_CONF = 0x04,
  REALTIME_REPORT = 0x05,
  LOCAL_TIME = 0x06,
  WIFI_TEST = 0x07,
  RECORD_REPORT = 0x08,
  SEND_COMMAND = 0x09,
  MODULE_FW_UPGRADE = 0x0a,
  SIGNAL_STRENGTH = 0x0b,
  MCU_FW_UPGRADE = 0x0c,
  MCU_FW_SIZE = 0x0d,
  MCU_FW_SEND = 0x0e,
  GET_DP_CACHE = 0x10,
};

enum class TuyaCommandTypeV3 : uint8_t {
  HEARTBEAT = 0x00,
  PRODUCT_QUERY = 0x01,
  CONF_QUERY = 0x02,
  WIFI_STATE = 0x03,
  WIFI_RESET = 0x04,
  WIFI_SELECT = 0x05,
  DATAPOINT_DELIVER = 0x06,
  DATAPOINT_REPORT = 0x07,
  DATAPOINT_QUERY = 0x08,
  WIFI_TEST = 0x0E,
};

enum class TuyaInitState : uint8_t {
  INIT_HEARTBEAT = 0x00,
  INIT_PRODUCT,
  INIT_CONF,
  INIT_WIFI,
  INIT_DATAPOINT,
  INIT_DONE,
};

class Tuya : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void register_listener(uint8_t datapoint_id, const std::function<void(TuyaDatapoint)> &func);
  void set_datapoint_value(TuyaDatapoint datapoint);

 protected:
  void handle_char_(uint8_t c);
  void handle_datapoints_(const uint8_t *buffer, size_t len);
  bool validate_message_();

  void handle_command_(TuyaCommandTypeV0 command, uint8_t version, const uint8_t *buffer, size_t len);
  void handle_command_(TuyaCommandTypeV3 command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_command_(uint8_t command, const uint8_t *buffer, uint16_t len);
  void send_command_(TuyaCommandTypeV0 command, const uint8_t *buffer, uint16_t len) {
    this->send_command_((uint8_t) command, buffer, len);
  }
  void send_command_(TuyaCommandTypeV3 command, const uint8_t *buffer, uint16_t len) {
    this->send_command_((uint8_t) command, buffer, len);
  }
  void send_empty_command_(TuyaCommandTypeV0 command) { this->send_command_(command, nullptr, 0); }
  void send_empty_command_(TuyaCommandTypeV3 command) { this->send_command_(command, nullptr, 0); }

  TuyaInitState init_state_ = TuyaInitState::INIT_HEARTBEAT;
  WifiState wifi_state_ = WifiState::DISCONNECTED;
  int gpio_status_ = -1;
  int gpio_reset_ = -1;
  std::string product_ = "";
  int version_ = -1;
  std::vector<TuyaDatapointListener> listeners_;
  std::vector<TuyaDatapoint> datapoints_;
  std::vector<uint8_t> rx_message_;
};

}  // namespace tuya
}  // namespace esphome
