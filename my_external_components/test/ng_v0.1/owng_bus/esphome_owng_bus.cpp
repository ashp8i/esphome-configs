#include "esphome_owng_bus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace owng_bus {

static const char *TAG = "owng.bus";

void ESPHomeOneWireNG::setup() {
  ESPHomeComponent::setup();  // Add this
  
  if (this->split_io == OneWireBusPinConfig::ONEWIRE_BUS_PIN_SINGLE) {
    this->pin_->setup();  
  } else {
    this->input_pin_->setup();
    this->output_pin_->setup();
  }
}

void ESPHomeOneWireNG::dump_config() {
  ESP_LOGD(TAG, "  pin: %d", this->pin_->pin());  
  ESP_LOGD(TAG, "  input_pin: %d", this->input_pin_->pin());
  ESP_LOGD(TAG, "  output_pin: %d", this->output_pin_->pin());
}

// void ESPHomeOneWireNG::dump_config() {
//   ESP_LOGD(TAG, "Configuration:");
//   ESP_LOGD(TAG, "  split_io: %d", static_cast<int>(this->split_io));
//   ESP_LOGD(TAG, "  input_pin: %d", this->input_pin_->get_pin());
//   ESP_LOGD(TAG, "  output_pin: %d", this->output_pin_->get_pin());
// }

void ESPHomeOneWireNG::update() {
  // Implement 1-wire communication logic here
}

void ESPHomeOneWireNG::set_pin(InternalGPIOPin *pin) {
  if (pin != nullptr) {
    this->pin_ = pin;
    ESP_LOGD(TAG, "Set pin to %d", pin->pin());  
  } 
}

void ESPHomeOneWireNG::set_in_pin(InternalGPIOPin *in_pin) {
  if (in_pin != nullptr) {
    this->input_pin_ = in_pin;
    ESP_LOGD(TAG, "Set input_pin to %d", in_pin->pin());
  }
}

void ESPHomeOneWireNG::set_out_pin(InternalGPIOPin *out_pin) {
  if (out_pin != nullptr) {
    this->output_pin_ = out_pin;
    ESP_LOGD(TAG, "Set output_pin to %d", out_pin->pin());
  } 
}

void ESPHomeOneWireNG::set_split_io(OneWireBusPinConfig split_io) {
  if (split_io != this->split_io) {
    this->split_io = split_io;
    ESP_LOGD(TAG, "Set split_io to %d", static_cast<int>(split_io));
  }
}

}  // namespace owng_bus
}  // namespace esphome
