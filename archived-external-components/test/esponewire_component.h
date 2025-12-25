#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <vector>

namespace esphome {
namespace onewire {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const int ONE_WIRE_ROM_SEARCH;

class ESPOneWireComponent : public PollingComponent {
 public:
  explicit ESPOneWireComponent(InternalGPIOPin *pin);              // YAML pin setup pointer?
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }               // Set pin
  void register_callback(std::function<void(uint64_t)> callback);  // Register Device Callback
  //  void register_Device(OneWireBusDevice *device); // Register Device Callback

  void setup() override;                                                      // Component Setup
  void dump_config() override;                                                // Read Configuration
  float get_setup_priority() const override { return setup_priority::DATA; }  // Component Setup priority

  void update() override;  // Update Method
  // std::vector<uint64_t> get_found_devices() { return found_devices_; } // Vector for returning found devices
  // void set_parent(OneWireBusComponent *parent) { parent_ = parent; } // Helper to get a pointer to the address as
  // uint8_t.
  uint8_t *get_address8();  // Helper to create (and cache) the name for this Device. For example "0xfe0000031f1eaf29".
  const std::string &get_address_name();  // Address Name Return Method
  void set_address(uint64_t address);     // Set the 64-bit unsigned address for this Device.
  optional<uint8_t> get_index() const {
    if (index_.has_value()) {
      return index_.value();
    } else {
      return {};  // return an empty optional
    }
  }                               // Get the index of this Device. (0 if using address.)
  void set_index(uint8_t index);  // Set the index, if using index, address will be set after setup.
  bool setup_device();            // Device Setup
  bool read_scratch_pad();
  bool check_scratch_pad();
  std::string unique_id();  // override;
  void process_scratch_pad(
  const std::vector<uint8_t> &scratch_pad);  // Add this new function to process the scratch pad data

 protected:
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
  inline uint8_t *rom_number8_();      // Helper for internal 64-bit unsigned rom number as a 8-bit integer pointer.
  ISRInternalGPIOPin isr_pin_;         // ISR Pin Pointer
  InternalGPIOPin *pin_;               // Pin Pointer
  ESPOneWireComponent *one_wire_;      // one wire pointer
  std::vector<ESPOneWireComponent *> devices_;            // device prefix
  std::vector<uint64_t> found_devices_;                   // Vector for devices returned from a search
  uint8_t last_discrepancy_{0};                           // Scratch Pad Error Checking
  bool last_device_flag_{false};                          // last device?
  uint64_t rom_number_{0};                                // Unsigned 64-bit ROM Number
  std::vector<std::function<void(uint64_t)>> callbacks_;  // Write
  uint64_t address_;                                      // Unsigned 64-bit ROM Address
  optional<uint8_t> index_;                               // Unsigned 64-bit ROM Address
  std::string address_name_;                              // Device Name String
  uint8_t scratch_pad_[9] = {
      0,
  };  // Unsigned 8-bit Scratch Pad Store
};

}  // namespace onewire
}  // namespace esphome
