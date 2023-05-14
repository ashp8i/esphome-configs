#pragma once

#include "onewire_bus.h"

namespace esphome {
namespace onewire {

class OneWireDevice {
 public:
  OneWireDevice() = default;
  
  void set_onewire_address(uint8_t address) { address_ = address; }
  void set_onewire_bus(OneWireBus *bus) { bus_ = bus; }
  
  ErrorCode read(uint8_t *data, size_t len) { return bus_->read(address_, data, len); }
  ErrorCode read_ROM(uint8_t *data) { return bus_->read_rom(address_, data); }
  
  ErrorCode write(const uint8_t *data, uint8_t len, bool parasite_power = false) { 
    return bus_->write(address_, data, len, parasite_power); 
  }
  ErrorCode write_ROM(const uint8_t *data) { return bus_->write_rom(address_, data); }
  
  // Compat APIs
  
  bool read_bytes(uint8_t *data, uint8_t len) { 
    return read(data, len) == ERROR_OK; 
  }
  bool read_bytes_raw(uint8_t *data, uint8_t len) { return read(data, len) == ERROR_OK; }
  
  template<size_t N> optional<std::array<uint8_t, N>> read_bytes() {
    std::array<uint8_t, N> res;
    if (!this->read_bytes(res.data(), N)) {
      return {};
    }
    return res;
  }
  
  bool read_rom(uint8_t *rom) { return read_ROM(rom) == ERROR_OK; }
  
  bool write_bytes(const uint8_t *data, uint8_t len, bool parasite_power = false) {
    return write(data, len, parasite_power) == ERROR_OK; 
  }
  bool write_rom(const uint8_t *rom) { return write_ROM(rom) == ERROR_OK; }
  
 protected:
  uint8_t address_{0x00};
  OneWireBus *bus_{nullptr};
};

}  // namespace onewire
}  // namespace esphome