#pragma once

#include "esphome/core/hal.h"
#include <vector>

namespace esphome {
namespace onewirebus {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const uint8_t ONE_WIRE_ROM_SEARCH;

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
  bool write_bit(bool bit_to_write);

  /// Read a single bit from the bus, takes about 70µs
  bool read_bit();

  // Add a new method to log the status of communication attempts.
  void log_communication_status();

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
  uint32_t failed_communication_attempts_; 

 protected:
  ISRInternalGPIOPin pin_;
  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint8_t rom_number_[8] = {0};
};

}  // namespace onewirebus
}  // namespace esphome
