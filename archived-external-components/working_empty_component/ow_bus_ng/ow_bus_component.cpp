#include "ow_bus_component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ow_bus_ng {

static const char *const TAG = "owbus.ng";

void ESPHomeOneWireNGComponent::set_single_pin(InputPin *pin) {
  this->pin_ = pin;  // InputPin resolves to InternalGPIOPin
}

void ESPHomeOneWireNGComponent::set_split_io(OutputPin *tx_pin, InputPin *rx_pin) {
  this->input_pin_ = rx_pin;
  this->output_pin_ = tx_pin;
}

// Constructor definitions here
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent() : pin_config_(OneWirePinConfig::SINGLE_PIN) {}

ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(InternalGPIOPin *pin)
    : pin_(pin), pin_config_(OneWirePinConfig::SINGLE_PIN) {}

void ESPHomeOneWireNGComponent::setup() {
  Component::setup();  // Call parent class setup()

  ESP_LOGCONFIG(TAG, "Setting up ESPHomeOneWireNGComponent...");

  switch (this->pin_config_) {
    case OneWirePinConfig::SINGLE_PIN:
      if (this->pin_ != nullptr) {
        this->pin_->setup();
      }
      break;
    case OneWirePinConfig::SPLIT_IO:
      this->input_pin_->setup();
      this->output_pin_->setup();
      break;
  }

  // Initialize 1-Wire bus and search for devices
  // ...
}

void ESPHomeOneWireNGComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Configuration:");

  switch (this->pin_config_) {
    case OneWirePinConfig::SINGLE_PIN:
      ESP_LOGCONFIG(TAG, "  pin_config: SINGLE_PIN");
      if (this->pin_ != nullptr) {
        ESP_LOGCONFIG(TAG, "  pin: %d", this->pin_->get_pin());
      }
      break;

    case OneWirePinConfig::SPLIT_IO:
      ESP_LOGCONFIG(TAG, "  pin_config: SPLIT_IO");
      ESP_LOGCONFIG(TAG, "  input_pin: %d", this->input_pin_->get_pin());
      ESP_LOGCONFIG(TAG, "  output_pin: %d", this->output_pin_->get_pin());
      break;
  }
}

}  // namespace ow_bus_ng
}  // namespace esphome
