#ifndef ONEWIREBUS_H_ 
#define ONEWIREBUS_H_

#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h" 
#include "esphome/components/sensor/sensor.h"

#include <vector>

namespace esphome {
namespace onewirebus {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const int ONE_WIRE_ROM_SEARCH;

class OneWireBus {
 public:
  explicit OneWireBus(InternalGPIOPin *pin);

  /** Reset the bus, should be done before all write operations.
   * Takes approximately 1ms.
   * @return Whether the operation was successful.
   */
  bool reset();
  void write_bit(bool bit);             /// Write a single bit to the bus, takes about 70µs.
  bool read_bit();                      /// Read a single bit from the bus, takes about 70µs
  void write8(uint8_t val);             /// Write a word to the bus. LSB first.
  void write64(uint64_t val);           /// Write a 64 bit unsigned integer to the bus. LSB first.
  void skip();                          /// Write a command to the bus that addresses all devices by skipping the ROM.
  uint8_t read8();                      /// Read an 8 bit word from the bus.
  uint64_t read64();                    /// Read an 64-bit unsigned integer from the bus.
  void select(uint64_t address);        /// Select a specific address on the bus for the following command.
  void reset_search();                  /// Reset the device search.
  uint64_t search();                    /// Search for a 1-Wire device on the bus. Returns 0 if all devices have been found.
  std::vector<uint64_t> search_vec();   /// Helper that wraps search in a std::vector.

 protected:
  // inline uint8_t *rom_number8_();       /// Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.

  ISRInternalGPIOPin pin_;
  // uint8_t last_discrepancy_{0};
  // bool last_device_flag_{false};
  // uint64_t rom_number_{0};
};

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

class OneWireTemperatureSensor : public sensor::Sensor {
 public:
  void set_parent(OneWireBusComponent *parent) { parent_ = parent; }  /// Helper to get a pointer to the address as uint8_t.

  uint8_t *get_address8();                         /// Helper to get a pointer to the address as uint8_t
  const std::string &get_address_name();           /// Helper to create (and cache) the name for this sensor. For example "0xfe0000031f1eaf29".
  void set_address(uint64_t address);              /// Set the 64-bit unsigned address for this sensor.
  optional<uint8_t> get_index() const;             /// Get the index of this sensor. (0 if using address.)
  void set_index(uint8_t index);                   /// Set the index of this sensor. If using index, address will be set after setup.
  uint8_t get_resolution() const;                  /// Get the set resolution for this sensor.
  void set_resolution(uint8_t resolution);         /// Set the resolution for this sensor.
  uint16_t millis_to_wait_for_conversion() const;  /// Get the number of milliseconds we have to wait for the conversion phase.

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
  uint8_t scratch_pad_[9] = {0};
};

}  // namespace onewirebus
}  // namespace esphome

#endif //ONEWIREBUS_H_
