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
  inline uint8_t *rom_number8_();       /// Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.

  ISRInternalGPIOPin pin_;
  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint64_t rom_number_{0};
};

class OneWireBusComponent : public Component {
  public:
    void setup() override;
    void dump_config() override;
    void loop() override;

    OneWireBus *get_bus() { return this->bus_; }
    const std::vector<uint64_t> &get_addresses() { return this->addresses_; }

  protected:
    OneWireBus *bus_;
    std::vector<uint64_t> addresses_;
    std::vector<OneWireBusDevice *> devices_; 
};

class OneWireBusDevice {
  public:
    OneWireBusDevice(OneWireBusComponent *parent);

    void setup(); 
    void dump_config();

  protected:
    OneWireBusComponent *parent_;
};

}  // namespace onewirebus
}  // namespace esphome
