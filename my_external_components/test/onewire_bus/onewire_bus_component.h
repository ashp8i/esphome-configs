#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include <vector>

namespace esphome {
namespace onewire_bus {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const int ONE_WIRE_ROM_SEARCH;

class OneWireBusDevice;

class OneWireBusComponent : public Component {
 public:
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }
  void register_Device(OneWireBusDevice *device);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void update() override;
  std::vector<uint64_t> get_found_devices() { return found_devices_; }

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

  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint64_t rom_number_{0};

  friend OneWireBusDevice;

  InternalGPIOPin *pin_;
  OneWireBuComponent *one_wire_;
  std::vector<OneWireBusDevice *> devices_;
  std::vector<uint64_t> found_devices_;
};

/// Internal class that helps us create multiple Devices for one instance of a 1-Wire Bus.
class OneWireBusDevice : public Device::Device {
 public:
  void set_parent(OneWireBusComponent *parent) { parent_ = parent; }
  /// Helper to get a pointer to the address as uint8_t.
  uint8_t *get_address8();
  /// Helper to create (and cache) the name for this Device. For example "0xfe0000031f1eaf29".
  const std::string &get_address_name();

  /// Set the 64-bit unsigned address for this Device.
  void set_address(uint64_t address);
  /// Get the index of this Device. (0 if using address.)
  optional<uint8_t> get_index() const {
  if (index_.has_value()) {
    return index_.value();
  } else {
    return {}; // return an empty optional
  }
  }
  /// Set the index of this Device. If using index, address will be set after setup.
  void set_index(uint8_t index);
  /// Get the set resolution for this Device.
  uint8_t get_resolution() const;
  /// Set the resolution for this Device.
  void set_resolution(uint8_t resolution);
  /// Get the number of milliseconds we have to wait for the conversion phase.
  uint16_t millis_to_wait_for_conversion() const;

  bool setup_device();

  bool read_scratch_pad();

  bool check_scratch_pad();

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

}  // namespace onewire_bus
}  // namespace esphome