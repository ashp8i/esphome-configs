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
  explicit OneWireBusComponent(InternalGPIOPin *pin);              // YAML pin setup pointer?
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }               // Set pin
  void register_Device(OneWireBusDevice *device);  // Register Device Callback
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void update(); //override;
  std::vector<uint64_t> get_found_devices() { return found_devices_; }
  bool reset();                        // Reset, used before write operations performed, takes 1ms, @return validation
  void write_bit(bool bit);            // Write single bit 70µs.
  bool read_bit();                     // Read single bit 70µs.
  void write8(uint8_t val);            // Write a word, LSB first.
  void write64(uint64_t val);          // Write a 64 bit unsigned integer, LSB first.
  void skip();                         // Write command to all devices by skipping the ROM.
  uint8_t read8();                     /// Read an 8 bit word.
  uint64_t read64();                   // Read an 64-bit unsigned integer
  void select(uint64_t address);       // Select a specific address for following command
  void reset_search();                 // Reset the device search.
  uint64_t search();                   // Search for devices, Returns 0 if all devices have been found.
  std::vector<uint64_t> search_vec();  // Helper that wraps search in a std::vector.

 protected:
  inline uint8_t *rom_number8_();      // Helper for internal 64-bit unsigned rom number as a 8-bit integer pointer.
  ISRInternalGPIOPin isr_pin_;         // ISR Pin Pointer
  InternalGPIOPin *pin_;               // Pin Pointer
  OneWireBusComponent *one_wire_;      // one wire pointer
  std::vector<OneWireBusComponent *> devices_;            // device prefix
  std::vector<uint64_t> found_devices_;                   // Vector for devices returned from a search

  std::vector<std::function<void(uint64_t)>> callbacks_;  // Write
  uint64_t address_;                                      // Unsigned 64-bit ROM Address
  optional<uint8_t> index_;                               // Unsigned 64-bit ROM Address
  std::string address_name_;                              // Device Name String
  /// Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.
  inline uint8_t *rom_number8_();

  uint8_t last_discrepancy_{0};                           // Scratch Pad Error Checking
  bool last_device_flag_{false};                          // last device?
  uint64_t rom_number_{0};                                // Unsigned 64-bit ROM Number

  friend OneWireBusDevice;

  InternalGPIOPin *pin_;
  OneWireBusComponent *one_wire_;
  std::vector<OneWireBusDevice *> devices_;
  std::vector<uint64_t> found_devices_;
};

/// Internal class that helps us create multiple Devices for one instance of a 1-Wire Bus.
class OneWireBusDevice : public onewire_bus::OneWireBusComponent {
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

  std::string unique_id(); //override;

    // Add this new function to process the scratch pad data
  void process_scratch_pad(const std::vector<uint8_t>& scratch_pad);

 protected:
  OneWireBusComponent *parent_;

  uint64_t address_;

  optional<uint8_t> index_;

  uint8_t resolution_;

  std::string address_name_;

  uint8_t scratch_pad_[9] = {
      0,
  };

  // Add this new function to parse the temperature value from the scratch pad
  float parse_temperature(const std::vector<uint8_t>& scratch_pad) const;
};

}  // namespace onewire_bus
}  // namespace esphome
