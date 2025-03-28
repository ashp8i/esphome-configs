#pragma once

#include "esphome/core/hal.h"
#include <vector>

namespace esphome {
namespace dallas {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const int ONE_WIRE_ROM_SEARCH;

class ESPOneWire {
 public:
  explicit ESPOneWire(InternalGPIOPin pin_or_pins); 

  /** Reset the bus, should be done before all write operations.
   *
   * Takes approximately 1ms.
   *
   * @return Whether the operation was successful.
   */
  bool reset();

  void SplitIOReset();

  /// Write a single bit to the bus, takes about 70µs.
  void write_bit(bool bit);

  void SplitIOWriteBit(bool bit);

  /// Read a single bit from the bus, takes about 70µs
  bool read_bit();

  bool SplitIOReadBit();

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
  union {  
    ISRInternalGPIOPin *pin_;
    struct {
      ISRInternalGPIOPin *input_pin_; 
      ISRInternalGPIOPin *output_pin_; 
    };
  };
  
  uint8_t last_discrepancy_;
  bool last_device_flag_;
  uint64_t rom_number_;
  
  /// Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.
  uint8_t *rom_number8_(); 
  
  bool split_io_;
};

}  // namespace dallas
}  // namespace esphome
