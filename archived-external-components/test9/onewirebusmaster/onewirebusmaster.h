#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h" 
#include "esphome/components/sensor/sensor.h"

#include <vector>
#include <map>

namespace esphome {
namespace onewirebus {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const int ONE_WIRE_ROM_SEARCH;

class OneWireBusDevice {
 public:
  OneWireBusDevice(OneWireBusComponent* parent) : parent_(parent) {};
  InternalGPIOPin* get_pin() const;
  uint64_t address;             /// Address property
  virtual void setup() = 0;     /// Setup function
  virtual void dump_config() = 0;   /// Dump the configuration to the log.
  virtual void loop() = 0;      /// Loop function

 protected:
  OneWireBusComponent *parent_;       /// The parent component.       
  // uint8_t index_{};                   /// The index of this device.
}; 

class OneWireBusComponent : public Component {
 public:
  explicit OneWireBusComponent(InternalGPIOPin *pin);      /// Constructor.
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
  const uint8_t* get_address8() const;             /// Helper to get a pointer to the address as uint8_t.
  std::string get_address_name(uint64_t address);  /// Helper to create (and cache) the name for this sensor. For example "0xfe0000031f1eaf29".
  void set_address(uint64_t address);   /// Set the 64-bit unsigned address for this sensor.
  uint8_t get_index() const;  /// Get the index of this sensor. (0 if using address.)
  void set_index(uint8_t index);        /// Set the index of this sensor. If using index, address will be set after setup.
  void add_device(std::unique_ptr<OneWireBusDevice> device);    /// Register a device with the bus. Take ownership of the pointer
  void add_device(OneWireBusDevice& device);    /// Register a device with the bus.
  // void register_sensor(OneWireTemperatureSensor *sensor); /// Register a sensor with the bus.
  // OneWireBusComponent *get_bus() { return this->bus_; }   /// Get the bus this component is registered to.
  std::vector<uint64_t> addresses_;     /// The addresses of the devices found. 
  const InternalGPIOPin* get_pin() const; /// Get the pin used to communicate with the bus.
  void setup() override;              /// Called once when the component is registered.
  void dump_config() override;        /// Dump the configuration to the log.
  void loop() override;               /// Called repeatedly by the main loop.

 protected:
  inline uint8_t *rom_number8_();       /// Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.
  ISRInternalGPIOPin pin_;              /// The pin used to communicate with the bus.
  uint8_t last_discrepancy_{0};         /// The last discrepancy between the rom number and the actual rom number.
  bool last_device_flag_{false};        /// Whether the last device was found.
  uint64_t rom_number_{0};              /// The rom number of the last device found.
  // OneWireBusComponent *bus_;            /// The bus this component is registered to.
  // std::map<OneWireBusDevice*, uint8_t> device_indexes;    /// The device indexes of the devices found.
  std::vector<std::unique_ptr<OneWireBusDevice>> devices_; /// The devices found.
  std::string address_name_;                  /// The name of the address.
  void add_device(OneWireBusDevice *device);  /// Register a device with the bus.
};

}  // namespace onewirebus
}  // namespace esphome
