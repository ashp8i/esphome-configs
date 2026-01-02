#pragma once

#include <cinttypes>
#include <vector>
#include <deque>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"
#endif

namespace esphome {
namespace tuya {

using namespace uart;  // Add this

enum TuyaInitState {
  INIT_HEARTBEAT = 0,
  INIT_PRODUCT = 1,
  INIT_CONF = 2,
  INIT_DONE = 3,
};

enum TuyaDatapointType {
  RAW = 0x00,
  BOOLEAN = 0x01,
  INTEGER = 0x02,
  STRING = 0x03,
  ENUM = 0x04,
  BITMASK = 0x05,
};

enum TuyaCommandType {
  HEARTBEAT = 0x00,
  PRODUCT_QUERY = 0x01,
  CONF_QUERY = 0x02,
  WIFI_STATE = 0x03,
  DATAPOINT_DELIVER = 0x07,
  DATAPOINT_REPORT_ASYNC = 0x07,
  DATAPOINT_REPORT_SYNC = 0x08,
  DATAPOINT_QUERY = 0x09,
  DATAPOINT_REPORT_ACK = 0x0A,
  WIFI_TEST = 0x0B,
  WIFI_RSSI = 0x0C,
  LOCAL_TIME_QUERY = 0x1C,
  GET_NETWORK_STATUS = 0x24,
  EXTENDED_SERVICES = 0x25,
  VACUUM_MAP_UPLOAD = 0x62,
};

enum TuyaWifiState {
  WIFI_STATE_NO_AP = 0x00,
  WIFI_STATE_AP = 0x01,
  WIFI_STATE_CONNECTED = 0x02,
  WIFI_STATE_CONNECTED_CLOUD = 0x03,
  WIFI_STATE_CONNECTED_CLOUD_FAILED = 0x04,
};

struct TuyaDatapoint {
  uint8_t id;
  TuyaDatapointType type;
  size_t len;
  union {
    bool value_bool;
    uint32_t value_uint;
    int32_t value_int;
    uint8_t value_enum;
    uint32_t value_bitmask;
  };
  std::vector<uint8_t> value_raw;
  std::string value_string;
};

struct TuyaCommand {
  TuyaCommandType cmd;
  std::vector<uint8_t> payload;
};

struct TuyaDatapointListener {
  uint8_t datapoint_id;
  std::function<void(TuyaDatapoint)> on_datapoint;
};

class Tuya : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;

  // Configuration
  void set_status_pin(GPIOPin *status_pin) { this->status_pin_ = status_pin; }

#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
#endif
  void add_ignore_mcu_update_on_datapoints(uint8_t ignore_mcu_update_on_datapoints) {
    this->ignore_mcu_update_on_datapoints_.push_back(ignore_mcu_update_on_datapoints);
  }
  void add_on_initialized_callback(std::function<void()> callback) {
    this->initialized_callback_.add(std::move(callback));
  }

  // Datapoint operations
  void set_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void set_datapoint_value(uint8_t datapoint_id, bool value);
  void set_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void set_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void set_datapoint_value(uint8_t datapoint_id, uint8_t enum_value);
  void set_datapoint_value(uint8_t datapoint_id, uint32_t bitmask_value, uint8_t length);

  void force_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void force_datapoint_value(uint8_t datapoint_id, bool value);
  void force_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void force_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void force_datapoint_value(uint8_t datapoint_id, uint8_t enum_value);
  void force_datapoint_value(uint8_t datapoint_id, uint32_t bitmask_value, uint8_t length);

  optional<TuyaDatapoint> get_datapoint_value(uint8_t datapoint_id);

  // Event handling
  void register_listener(uint8_t datapoint_id, const std::function<void(TuyaDatapoint)> &func);
  void register_initialized_callback(std::function<void()> callback) {
    this->initialized_callback_.add(std::move(callback));
  }

  // WiFi operations
  void send_wifi_state(TuyaWifiState state);
  void send_wifi_reset();
  void send_factory_reset();
  void send_query_data();

  // Status
  TuyaInitState get_init_state() { return this->init_state_; }
  uint8_t get_protocol_version() { return this->protocol_version_; }
  const std::string &get_product() { return this->product_; }

  // Wrapper Methods for Existing ESPHome Tuya Component Compatibility
  void set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);

 protected:
  // Internal methods
  void process_input_buffer_();
  void process_frames_();
  void process_command_queue_();
  void handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void handle_datapoints_(const uint8_t *buffer, size_t len);
  void send_raw_command_(TuyaCommand command);
  void send_command_(const TuyaCommand &command);
  void send_empty_command_(TuyaCommandType command);
  void send_datapoint_command_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const std::vector<uint8_t> &data);
  void set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, uint32_t value,
                                    uint8_t length, bool forced);
  void set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced);
  void set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced);
  optional<TuyaDatapoint> get_datapoint_(uint8_t datapoint_id);


  uint8_t get_wifi_status_code_();
  uint8_t get_wifi_rssi_();
  void send_wifi_status_();
  void set_status_pin_();

  // State management
  optional<TuyaCommandType> expected_response_;
  std::deque<uint8_t> rx_message_;
  std::deque<TuyaCommand> command_queue_;
  std::vector<TuyaDatapoint> datapoints_;
  std::vector<TuyaDatapointListener> listeners_;
  uint32_t last_command_timestamp_{0};
  uint32_t last_rx_char_timestamp_{0};
  uint32_t handshake_start_time_{0};
  uint8_t init_retries_{0};

  // Configuration
  uint8_t protocol_version_{0xFF};
  uint8_t status_pin_reported_{255};
  uint8_t reset_pin_reported_{255};
  uint8_t wifi_status_{0xFF};
  std::string product_;
  std::vector<uint8_t> ignore_mcu_update_on_datapoints_;
  bool init_failed_{false};
  TuyaInitState init_state_{TuyaInitState::INIT_HEARTBEAT};

  // Callbacks
  CallbackManager<void()> initialized_callback_;

  // Hardware
  GPIOPin *status_pin_{nullptr};
#ifdef USE_TIME
  void send_local_time_();
  time::RealTimeClock *time_id_{nullptr};
  bool time_sync_callback_registered_{false};
#endif
  // bool frames_processed_{false};
};

}  // namespace tuya
}  // namespace esphome
