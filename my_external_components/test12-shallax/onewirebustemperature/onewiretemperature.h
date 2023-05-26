#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "onewirebus.h"

#include <vector>

namespace esphome {
namespace onewirebus {

class OneWireTemperatureSensor;

class OneWireBusComponent : public PollingComponent {
 public:
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void register_sensor(OneWireTemperatureSensor *sensor);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void update() override;

 protected:
  friend OneWireTemperatureSensor;

  InternalGPIOPin *pin_;
  OneWireBusComponent *one_wire_;
  std::vector<OneWireTemperatureSensor *> sensors_;
  std::vector<uint64_t> found_sensors_;
};

/// Internal class that helps us create multiple sensors for one Dallas hub.
class OneWireTemperatureSensor : public sensor::Sensor {
 public:
  void set_parent(OneWireBusComponent *parent) { parent_ = parent; }
  /// Helper to get a pointer to the address as uint8_t.
  uint8_t *get_address8();
  /// Helper to create (and cache) the name for this sensor. For example "0xfe0000031f1eaf29".
  const std::string &get_address_name();

  /// Set the 64-bit unsigned address for this sensor.
  void set_address(uint64_t address);
  /// Get the index of this sensor. (0 if using address.)
  optional<uint8_t> get_index() const;
  /// Set the index of this sensor. If using index, address will be set after setup.
  void set_index(uint8_t index);
  /// Get the set resolution for this sensor.
  uint8_t get_resolution() const;
  /// Set the resolution for this sensor.
  void set_resolution(uint8_t resolution);
  /// Get the number of milliseconds we have to wait for the conversion phase.
  uint16_t millis_to_wait_for_conversion() const;

  bool setup_sensor();
  bool read_scratch_pad();

  bool check_scratch_pad();

  float get_temp_c();

  std::string unique_id() override;

 protected:
  OneWireBusComponent *parent_;
  uint64_t address_;
  optional<uint8_t> index_;

  uint8_t resolution_;
  std::string address_name_;
  uint8_t scratch_pad_[9] = {
      0,
  };
};


/// This class implements support for the dallas ds18x20 series sensor.
class OneWireTemperatureComponent : public PollingComponent, public OneWirebus::OneWireDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }

  /// Set the oversampling value for the temperature sensor. Default is 16x.
  void set_temperature_oversampling(BME280Oversampling temperature_over_sampling);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  /// Read the temperature value and store the calculated ambient temperature in t_fine.
  float read_temperature_(const uint8_t *data, int32_t *t_fine);
  uint8_t read_u8_(uint8_t a_register);


  sensor::Sensor *temperature_sensor_{nullptr};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_CHIP_ID,
  } error_code_{NONE};

};

}  // namespace onewirebus
}  // namespace esphome
