#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "onewirebus.h"

#include <vector>

namespace esphome {
namespace ds18b20 {

class DS18B20TemperatureSensor;

class DS18B20Component : public PollingComponent {
 public:
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void register_sensor(DS18B20TemperatureSensor *sensor);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void update() override;

 protected:
  friend DS18B20TemperatureSensor;

  InternalGPIOPin *pin_;
  OneWireBus *one_wire_;
  std::vector<DS18B20TemperatureSensor *> sensors_;
  std::vector<uint64_t> found_sensors_;
};

class DS18B20TemperatureSensor : public sensor::Sensor {
 public:
  void set_parent(DS18B20Component *parent) { parent_ = parent; }
  uint8_t *get_address8();
  const std::string &get_address_name();

  void set_address(uint64_t address);
  optional<uint8_t> get_index() const;
  void set_index(uint8_t index);
  uint8_t get_resolution() const;
  void set_resolution(uint8_t resolution);
  uint16_t millis_to_wait_for_conversion() const;

  bool setup_sensor();
  bool read_scratch_pad();

  bool check_scratch_pad();

  float get_temp_c();

  std::string unique_id() override;

 protected:
  DS18B20Component *parent_;
  uint64_t address_;
  optional<uint8_t> index_;

  uint8_t resolution_;
  std::string address_name_;
  uint8_t scratch_pad_[9] = {
      0,
  };
};

}  // namespace ds18b20
}  // namespace esphome