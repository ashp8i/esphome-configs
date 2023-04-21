#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h" 
#include "esphome/components/gpio/gpio.h"
#include <vector>

namespace esphome {
namespace owng_bus {

enum class OneWireBusPinConfig {
  ONEWIRE_BUS_PIN_SINGLE,
  ONEWIRE_BUS_PIN_SPLIT
};

class ESPHomeOneWireNG : public Component {
 public:
  OneWireBusPinConfig split_io;   
  InternalGPIOPin *input_pin_ = nullptr;    
  InternalGPIOPin *output_pin_ = nullptr;   
  InternalGPIOPin *pin_ = nullptr;  
  
  ESPHomeOneWireNG(OneWireBusPinConfig split_io,     
                 InternalGPIOPin *input_pin = nullptr,    
                 InternalGPIOPin *output_pin = nullptr,   
                 InternalGPIOPin *pin = nullptr)
  {
    this->split_io = split_io; 
    this->input_pin_ = input_pin;
    this->output_pin_ = output_pin;
    this->pin_ = pin; 
  }
  
  void setup() override {
    if (this->split_io == OneWireBusPinConfig::ONEWIRE_BUS_PIN_SINGLE) {
      this->pin_->setup(); 
    } else {
      this->input_pin_->setup();
      this->output_pin_->setup(); 
    }
  }

  // float get_setup_priority() const override { return 1.0; }
  // void dump_config() override;
  void dump_config() { 
    ESP_LOGD(TAG, "  pin: %d", this->pin_->pin());  
    ESP_LOGD(TAG, "  input_pin: %d", this->input_pin_->pin());
    ESP_LOGD(TAG, "  output_pin: %d", this->output_pin_->pin());
  }
  
  // void update() override;
  void update();
  
  void ESPHomeOneWireNG::set_pin(InternalGPIOPin *pin) {
    if (pin != nullptr) {
      this->pin_ = pin;
      ESP_LOGD(TAG, "Set pin to %d", pin->get_pin());  
    } 
  }
  
  void set_in_pin(InternalGPIOPin *in_pin);
  void set_out_pin(InternalGPIOPin *out_pin);
  void set_split_io(OneWireBusPinConfig split_io);

  // void issue_reset() {}
  // void issue_skip_rom() {}
  // void issue_command(uint8_t) {}
  
 protected:
  OneWireBusPinConfig split_io;     // Renamed from OneWirePinType
  InternalGPIOPin *input_pin_{nullptr};   // For SPLIT pin type (input)
  InternalGPIOPin *output_pin_{nullptr};  // For SPLIT pin type (output)
  InternalGPIOPin *pin_{nullptr};      // For SINGLE pin type 
};

void issue_reset() {}
void issue_skip_rom() {} 
void issue_command(uint8_t) {}

} // namespace owng_bus
} // namespace esphome
