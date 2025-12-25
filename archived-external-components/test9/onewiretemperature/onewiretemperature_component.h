#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"  
#include "../onewirebus/onewirebus.h"

namespace esphome {
namespace onewiretemperature {

class OneWireTemperature : public Component, public PollingComponent {
 public:
  void set_parent(OneWireBusComponent *parent) { this->parent_ = parent; }
  
  uint8_t get_resolution() const { return this->resolution_; }
  void set_resolution(uint8_t resolution) { this->resolution_ = resolution; }
  uint8_t *get_address8() { /* ... */ } 
  const std::string &get_address_name() { /* ... */ }
  void set_address(uint64_t address) { this->address_ = address; }
  
  void setup() override;
  void dump_config() override;
  void update() override;
  sensor::State get_state() override;
  std::string unit_of_measurement() override;  

 protected: 
  uint64_t get_address() const;    
  std::string get_model_name() const;
  bool validate_register();  
  
  OneWireBusComponent *parent_;
  // Register access 
  uint8_t get_register(uint8_t index) const;
  void set_register(uint8_t index, uint8_t value); 
  bool read_register();
  float get_temp_c();
  std::string unique_id() override;
  
  void register_temperature(OneWireTemperature *temperature);  
}; 

}  // namespace onewiretemperature
}  // namespace esphome