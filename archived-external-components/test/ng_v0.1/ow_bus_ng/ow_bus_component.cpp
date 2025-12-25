#include "ow_bus_component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ow_bus_ng {

static const char *TAG = "owbus.ng";  

void ESPHomeOneWireNGComponent::setup() {       
  ESP_LOGCONFIG(TAG, "Setting up ESPHomeOneWireNGComponent...");   
  
  if (this->split_io_ == OneWireBusPinConfig::ONEWIRE_BUS_PIN_SINGLE) {
    if (this->pin_ != nullptr) {  
      this->pin_->setup();     
    } 
  } else if (this->split_io_ == OneWireBusPinConfig::ONEWIRE_BUS_PIN_SPLIT) {
    this->input_pin_->setup();
    this->output_pin_->setup();
  }
  
  this->init_ow_bus();   
  
  // std::vector<uint64_t> raw_devices;  
}  

void ESPHomeOneWireNGComponent::init_ow_bus() {
  ESP_LOGD(TAG, "OneWire bus initialized! Scanning for devices...");
  
  if (this->split_io_ == OneWireBusPinConfig::ONEWIRE_BUS_PIN_SINGLE) {
    if (this->pin_ != nullptr) {
      this->pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
      delayMicroseconds(480);
      // Scan bus and get list of devices
      // raw_devices = ...  
    } 
  } else if (this->split_io_ == OneWireBusPinConfig::ONEWIRE_BUS_PIN_SPLIT) {
    this->input_pin_->pin_mode(gpio::FLAG_INPUT);
    this->output_pin_->pin_mode(gpio::FLAG_OUTPUT);
    // Scan bus and get list of devices
    // raw_devices = ...
  }
}

void ESPHomeOneWireNGComponent::set_split_io(OneWireBusPinConfig split_io) {
  if (split_io != this->split_io_) {
    this->split_io_ = split_io; 
    
    if (this->split_io_ == OneWireBusPinConfig::ONEWIRE_BUS_PIN_SINGLE) {
      if (this->pin_ != nullptr) {
        this->pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
      }
    } 
    
    else if (this->split_io_ == OneWireBusPinConfig::ONEWIRE_BUS_PIN_SPLIT && this->input_pin_ != nullptr && this->output_pin_ != nullptr) {
      this->input_pin_->pin_mode(gpio::FLAG_INPUT);
      this->output_pin_->pin_mode(gpio::FLAG_OUTPUT);
    }
    
    ESP_LOGD(TAG, "Set split_io to %d", static_cast<int>(split_io));
  }
}

void ESPHomeOneWireNGComponent::dump_config() {
  ESP_LOGD(TAG, "Configuration:");
  ESP_LOGD(TAG, "  split_io: %d", static_cast<int>(this->split_io_));
  if (this->pin_ != nullptr) {
    ESP_LOGD(TAG, "  pin: %d", this->pin_->get_pin());
  }
  if (this->input_pin_ != nullptr) {
    ESP_LOGD(TAG, "  input_pin: %d", this->input_pin_->get_pin());
  }
  if (this->output_pin_ != nullptr) {
    ESP_LOGD(TAG, "  output_pin: %d", this->output_pin_->get_pin());
  }
}

void ESPHomeOneWireNGComponent::set_pin(InternalGPIOPin *pin) {
  if (pin != nullptr) {
    this->pin_ = pin;
    this->pin_->setup();
    ESP_LOGD(TAG, "Set pin to %d", pin->get_pin());
  }
}

void ESPHomeOneWireNGComponent::set_in_pin(InternalGPIOPin *in_pin) {
  if (in_pin != nullptr) {
    this->input_pin_ = in_pin;
    this->input_pin_->setup();
    ESP_LOGD(TAG, "Set input_pin to %d", in_pin->get_pin());
  }
}

void ESPHomeOneWireNGComponent::set_out_pin(InternalGPIOPin *out_pin) {
  if (out_pin != nullptr) {
    this->output_pin_ = out_pin;
    this->output_pin_->setup();
    ESP_LOGD(TAG, "Set output_pin to %d", out_pin->get_pin());
  }
}

}  // namespace ow_bus_ng
}  // namespace esphome