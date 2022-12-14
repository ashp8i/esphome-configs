#pragma once

#include "esphome/core/component.h"
// #include "esphome/core/defines.h"
// #include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

// #ifdef USE_TIME
// #include "esphome/components/time/real_time_clock.h"
// #endif

namespace esphome {
namespace tuya_v0 {

enum class TuyaDatapointType : uint8_t {
  RAW = 0x00,      // variable length
  BOOLEAN = 0x01,  // 1 byte (0/1)
  INTEGER = 0x02,  // 4 byte
  STRING = 0x03,   // variable length
  ENUM = 0x04,     // 1 byte
  BITMASK = 0x05,  // 1/2/4 bytes
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
  std::string value_string;
};

struct TuyaDatapointListener {
  uint8_t datapoint_id;
  TuyaDatapointType type;
  std::function<void(TuyaDatapoint)> on_datapoint;
};

enum class TuyaCommandType : uint8_t {
  HEARTBEAT = 0x00,
  // PRODUCT_QUERY = 0x01,
  DATA_UPDATE = 0x02,
  WIFI_RESET = 0x06,
  // NETWORK_STATUS = 0x02,
  // WIFI_RESET = 0x03,
  WIFI_CONF = 0x04,
  REALTIME_REPORT = 0x05,
  // LOCAL_TIME = 0x06,
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

enum class TuyaInitState : uint8_t {
  INIT_HEARTBEAT = 0x01,
  INIT_PRODUCT,
  INIT_CONF,
  INIT_WIFI,
  INIT_DATAPOINT,
  INIT_DONE,
};

struct TuyaCommand {
  TuyaCommandType cmd;
  std::vector<uint8_t> payload;
};

class Tuya : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void register_listener(uint8_t datapoint_id, TuyaDatapointType type, const std::function<void(TuyaDatapoint)> &func);
  void set_datapoint_value(std::vector<uint8_t> &data);
  void add_ignore_mcu_update_on_datapoints(uint8_t ignore_mcu_update_on_datapoints) {
    this->ignore_mcu_update_on_datapoints_.push_back(ignore_mcu_update_on_datapoints);
  }

 protected:
  void handle_char_(uint8_t c);
  void handle_datapoint_(const uint8_t *buffer, size_t len);
  bool validate_message_();

  void handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_raw_command_(TuyaCommand command);
  void process_command_queue_();
  void send_command_(TuyaCommand command);
  void send_empty_command_(TuyaCommandType command);

  TuyaInitState init_state_ = TuyaInitState::INIT_HEARTBEAT;
  int gpio_status_ = -1;
  int gpio_reset_ = -1;
  uint32_t last_command_timestamp_ = 0;
  std::string product_ = "";
  int version_ = -1;
  std::vector<TuyaDatapointListener> listeners_;
  std::vector<TuyaDatapoint> datapoints_;
  std::vector<uint8_t> rx_message_;
  std::vector<uint8_t> ignore_mcu_update_on_datapoints_{};
  std::vector<TuyaCommand> command_queue_;
};

}  // namespace tuya_v0
}  // namespace esphome
