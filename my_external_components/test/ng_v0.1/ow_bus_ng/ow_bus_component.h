#pragma once

#include "esphome/core/hal.h"  
#include "esphome/core/component.h"
#include <vector>

namespace esphome {
namespace ow_bus_ng {

enum class OneWireBusPinConfig {
  ONEWIRE_BUS_PIN_SINGLE,
  ONEWIRE_BUS_PIN_SPLIT
};

class ESPHomeOneWireNGComponent : public Component {
 public:
  ESPHomeOneWireNGComponent(OneWireBusPinConfig pin_config,  
                             InternalGPIOPin *input_pin,  
                             InternalGPIOPin *output_pin,  
                             InternalGPIOPin *pin) 
  : pin_{pin}
  , input_pin_{input_pin}
  , output_pin_{output_pin}
  , split_io_{pin_config}
  {}   
  
  void setup() override;
  void dump_config() override;
  void update();

  void set_pin(InternalGPIOPin *pin);
  void set_in_pin(InternalGPIOPin *in_pin);
  void set_out_pin(InternalGPIOPin *out_pin);
  void set_split_io(OneWireBusPinConfig split_io);
  
  void issue_reset();
  void issue_skip_rom();
  void issue_command(uint8_t);

 protected:
  OneWireBusPinConfig split_io_;
  InternalGPIOPin *pin_{nullptr};
  InternalGPIOPin *input_pin_{nullptr};
  InternalGPIOPin *output_pin_{nullptr};
  
  void init_ow_bus();
   
};

} // namespace ow_bus_ng
} // namespace esphome

// ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(OneWireBusPinConfig pin_config, 
//                                InternalGPIOPin *input_pin,
//                                InternalGPIOPin *output_pin,
//                                InternalGPIOPin *pin) {
//   if (pin_config == ONEWIRE_BUS_PIN_SINGLE && pin != nullptr) {  
//     this->split_io_ = ONEWIRE_BUS_PIN_SINGLE; 
//   } 
  
//   else if (pin_config == ONEWIRE_BUS_PIN_SPLIT && input_pin != nullptr && output_pin != nullptr) {
//     this->split_io_ = ONEWIRE_BUS_PIN_SPLIT;
//     this->input_pin_ = input_pin;
//     this->output_pin_ = output_pin;
//   }
  
//   if (this->split_io_ == ONEWIRE_BUS_PIN_SINGLE) {
//     this->pin_ = pin; 
//   }
// }