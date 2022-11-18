#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif

namespace esphome {
namespace TuyaCustom {

/*
// protocol
#define TUYA_COVER_MAX_LEN 640  // Max length of message value
// #define TUYA_COVER_MAX_LEN 256  // Max length of message value
#define TUYA_COVER_BUFFER_LEN 6 // Length of serial buffer for header + type + length
#define TUYA_COVER_HEADER_LEN 2 // Length of fixed header

// enable/disable reversed motor direction
// Normal = header (55AA) + (00060005) + 050100010011 "(55AA00060005050100010011)
// Reversed = header (55AA) + (00060005) + 050100010112 "(55AA00060005050100010112)"
#define TUYA_COVER_DISABLE_REVERSING { 0x69, 0x01, 0x00, 0x01, 0x00 } //dpid = 105, type = bool, len = 1, value = disable
#define TUYA_COVER_ENABLE_REVERSING { 0x69, 0x01, 0x00, 0x01, 0x01 } //dpid = 105, type = bool, len = 1, value = enable
// Curtain commands
// Open = header (55AA) + (00060005) + 6604000100 "(55aa000600056604000100)"
// Close = header (55AA) + (00060005) + 6604000101 "(55aa000600056604000101)"
// Stop = header (55AA) + (00060005) + 6604000102 "(55AA000600056604000102)"
#define TUYA_COVER_OPEN { 0x66, 0x04, 0x00, 0x01, 0x00 } //dpid = 101, type = enum, len = 1, value = OPEN
#define TUYA_COVER_CLOSE { 0x66, 0x04, 0x00, 0x01, 0x01 } //dpid = 101, type = enum, len = 1, value = CLOSE
#define TUYA_COVER_STOP { 0x66, 0x04, 0x00, 0x01, 0x02 } //dpid = 101, type = enum, len = 1, value = STOP

#define TUYA_COVER_SET_POSITION { 0x65, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00 } //"65020004000000" //dpid = 2, type = value, len = 4,  value = 0x000000 + 1 byte (0x00-0x64)

static const char *TAG = "TuyaCOVER";
static const uint16_t TUYA_COVER_HEADER = 0x55AA;
//static const uint16_t TUYA_COVER_VERSION = 0x03;
static const uint16_t TUYA_COVER_VERSION = 0x00;

const uint8_t tuya_cover_enable_reversing[] = TUYA_COVER_ENABLE_REVERSING;
const uint8_t tuya_cover_disable_reversing[] = TUYA_COVER_DISABLE_REVERSING;
const uint8_t tuya_cover_open[] = TUYA_COVER_OPEN;
const uint8_t tuya_cover_close[] = TUYA_COVER_CLOSE;
const uint8_t tuya_cover_stop[] = TUYA_COVER_STOP;
static uint8_t tuya_cover_pos[] = TUYA_COVER_SET_POSITION;
*/

#define HEARTBEAT_INTERVAL_MS 10000

enum class TuyaCustomDatapointType : uint8_t {
  RAW = 0x00,      // variable length
  BOOLEAN = 0x01,  // 1 byte (0/1)
  INTEGER = 0x02,  // 4 byte
  STRING = 0x03,   // variable length
  ENUM = 0x04,     // 1 byte
  BITMASK = 0x05,  // 1/2/4 bytes
};

struct TuyaCustomDatapoint {
  uint8_t id;
  TuyaCustomDatapointType type;
  size_t len;
  union {
    bool value_bool;
    int value_int;
    uint32_t value_uint;
    uint8_t value_enum;
    uint32_t value_bitmask;
  };
  std::string value_string;
  std::vector<uint8_t> value_raw;
};

struct TuyaCustomDatapointListener {
  uint8_t datapoint_id;
  std::function<void(TuyaCustomDatapoint)> on_datapoint;
};

enum class TuyaCustomCommandType : uint8_t {
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
  LOCAL_TIME_QUERY = 0x1C,
};

enum class TuyaCustomInitState : uint8_t {
  INIT_HEARTBEAT = 0x00,
  INIT_PRODUCT,
  INIT_CONF,
  INIT_WIFI,
  INIT_DATAPOINT,
  INIT_DONE,
};

struct TuyaCustomCommand {
  TuyaCustomCommandType cmd;
  std::vector<uint8_t> payload;
};

class TuyaCustom : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void register_listener(uint8_t datapoint_id, const std::function<void(TuyaCustomDatapoint)> &func);
  void set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void set_status_pin(InternalGPIOPin *status_pin) { this->status_pin_ = status_pin; }
  void set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
  void force_set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void force_set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void force_set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void force_set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
  TuyaCustomInitState get_init_state();
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
#endif
  void add_ignore_mcu_update_on_datapoints(uint8_t ignore_mcu_update_on_datapoints) {
    this->ignore_mcu_update_on_datapoints_.push_back(ignore_mcu_update_on_datapoints);
  }
  void add_on_initialized_callback(std::function<void()> callback) {
    this->initialized_callback_.add(std::move(callback));
  }

 protected:
  void handle_char_(uint8_t c);
  void handle_datapoints_(const uint8_t *buffer, size_t len);
  optional<TuyaCustomDatapoint> get_datapoint_(uint8_t datapoint_id);
  bool validate_message_();

  void handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_raw_command_(TuyaCustomCommand command);
  void process_command_queue_();
  void send_command_(const TuyaCustomCommand &command);
  void send_empty_command_(TuyaCustomCommandType command);
  void set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaCustomDatapointType datapoint_type, uint32_t value,
                                    uint8_t length, bool forced);
  void set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced);
  void set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced);
  void send_datapoint_command_(uint8_t datapoint_id, TuyaCustomDatapointType datapoint_type, std::vector<uint8_t> data);
  void set_status_pin_();
  void send_wifi_status_();

#ifdef USE_TIME
  void send_local_time_();
  optional<time::RealTimeClock *> time_id_{};
#endif
  TuyaCustomInitState init_state_ = TuyaCustomInitState::INIT_HEARTBEAT;
  bool init_failed_{false};
  int init_retries_{0};
  uint8_t protocol_version_ = -1;
  optional<InternalGPIOPin *> status_pin_{};
  int status_pin_reported_ = -1;
  int reset_pin_reported_ = -1;
  uint32_t last_command_timestamp_ = 0;
  uint32_t last_rx_char_timestamp_ = 0;
  std::string product_ = "";
  std::vector<TuyaCustomDatapointListener> listeners_;
  std::vector<TuyaCustomDatapoint> datapoints_;
  std::vector<uint8_t> rx_message_;
  std::vector<uint8_t> ignore_mcu_update_on_datapoints_{};
  std::vector<TuyaCustomCommand> command_queue_;
  optional<TuyaCustomCommandType> expected_response_{};
  uint8_t wifi_status_ = -1;
  CallbackManager<void()> initialized_callback_{};
};

}  // namespace tuyacustom
}  // namespace esphome
