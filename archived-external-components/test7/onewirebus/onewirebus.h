#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>

namespace esphome {
namespace onewirebus {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const int ONE_WIRE_ROM_SEARCH;
extern const uint64_t ONE_WIRE_SEARCH_ERROR;

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

 private:
  // Error counters
  unsigned int successful_reset = 0;
  unsigned int failed_reset = 0;
  unsigned int successful_write = 0;
  unsigned int failed_write = 0;
  unsigned int successful_read = 0;
  unsigned int failed_read = 0;

 protected:
  ISRInternalGPIOPin pin_;
  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint8_t rom_number_[8] = {0};
};

class OneWireDevice : public Component {
 public:
  OneWireDevice(OneWireBus *one_wire, uint64_t address) : one_wire_(one_wire), address_(address), has_index_(false) {}

  void set_address(uint64_t address) { address_ = address; }
  uint64_t get_address() const { return address_; }

  void set_index(uint8_t index) { index_ = index; }
  uint8_t get_index() const { return index_; }
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

 protected:
  OneWireBus *one_wire_;
  uint64_t address_;
  uint8_t index_;
  bool has_index_;
};

class OneWireBusSensor : public sensor::Sensor, public OneWireDevice {
 public:
  OneWireBusSensor(OneWireBus *one_wire, uint64_t address) : OneWireDevice(one_wire, address) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_resolution(uint8_t resolution);
  uint8_t get_resolution() const;
  bool set_temp_resolution(uint8_t res);

  std::string unique_id() override;

 protected:
  uint8_t resolution_{12};
  static const uint8_t CONVERT_TEMPERATURE = 0x44;
  static const uint8_t READ_SCRATCHPAD = 0xBE;
  static const uint8_t WRITE_SCRATCHPAD = 0x4E;
  static const uint8_t COPY_SCRATCHPAD = 0x48;
};

}  // namespace onewirebus
}  // namespace esphome
