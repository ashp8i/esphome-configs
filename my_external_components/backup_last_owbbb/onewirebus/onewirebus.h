#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "onewirebus.h"

#include <vector>

namespace esphome {
namespace onewirebus {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const int ONE_WIRE_ROM_SEARCH;

class OneWireBus {
 public:
  explicit OneWireBus(InternalGPIOPin *pin);

  /** Reset the bus, should be done before all write operations.
   *
   * Takes approximately 1ms.
   *
   * @return Whether the operation was successful.
   */
  bool reset();

  /// Write a single bit to the bus, takes about 70µs.
  void write_bit(bool bit);

  /// Read a single bit from the bus, takes about 70µs
  bool read_bit();

  /// Write a word to the bus. LSB first.
  void write8(uint8_t val);

  /// Write a 64 bit unsigned integer to the bus. LSB first.
  void write64(uint64_t val);

  /// Write a command to the bus that addresses all devices by skipping the ROM.
  void skip();

  /// Read an 8 bit word from the bus.
  uint8_t read8();

  /// Read an 64-bit unsigned integer from the bus.
  uint64_t read64();

  /// Select a specific address on the bus for the following command.
  void select(uint64_t address);

  /// Reset the device search.
  void reset_search();

  /// Search for a 1-Wire device on the bus. Returns 0 if all devices have been found.
  uint64_t search();

  /// Helper that wraps search in a std::vector.
  std::vector<uint64_t> search_vec();

 protected:
  /// Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.
  inline uint8_t *rom_number8_();

  ISRInternalGPIOPin pin_;
  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint64_t rom_number_{0};
};

class OneWireTemperatureSensor;

class OneWireBusComponent : public PollingComponent {
 public:
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void register_sensor(OneWireTemperatureSensor *sensor);

  void setup();
  void dump_config();
  float get_setup_priority() const { return setup_priority::DATA; }
  void update();

 protected:
  friend OneWireTemperatureSensor;

  InternalGPIOPin *pin_;
  OneWireBus *one_wire_;
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

}  // namespace onewirebus
}  // namespace esphome
