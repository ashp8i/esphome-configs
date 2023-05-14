#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "onewirebus.h"

namespace esphome {
namespace onewiretemperature {

class OneWireTemperature : public sensor::Sensor {
 public:
  OneWireTemperature(OneWireBusComponent *parent);
  
  void setup() override;
  /// Print a message with the current config values.
  void dump_config() override;
  /// Update the sensor state.
  void update() override;
  /// Return the current sensor state.
  sensor::State get_state() override; 

  uint64_t get_address() const;    
  std::string get_model_name() const;
  ScratchPadValidation validate_scratch_pad();

 protected:
  OneWireBusComponent *parent_;
  // Scratchpad access 
  uint8_t get_reg(uint8_t index) const;
  void set_reg(uint8_t index, uint8_t value); 
  bool read_scratch_pad();
  float get_temp_c();
  std::string unique_id() override;
}; 

class OneWireBusComponent {
  void register_temperature(OneWireTemperature *temperature);
  void update();
};

}  // namespace onewiretemperature
}  // namespace esphome