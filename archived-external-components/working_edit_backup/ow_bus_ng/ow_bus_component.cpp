#include "ow_bus_component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace ow_bus_ng {

static const char *const TAG = "owbus.ng";

// void ESPHomeOneWireNGComponent::set_bitbang_single_pin(InternalGPIOPin *pin) {
//   this->pin_ = pin;
//   this->setup_methods_.push_back(ONEWIRE_SETUP_BITBANG_SINGLE_PIN);
// }

// void ESPHomeOneWireNGComponent::set_bitbang_split_io(InternalGPIOPin *input_pin, InternalGPIOPin *output_pin) {
//   this->input_pin_ = rx_pin;
//   this->output_pin_ = tx_pin;
//   this->setup_methods_.push_back(ONEWIRE_SETUP_BITBANG_SPLIT_IO);
// }

// void ESPHomeOneWireNGComponent::set_uart_bus_single_pin(UARTComponent *uart, GPIOPin *rx_pin, GPIOPin *tx_pin) {
//   this->uart_ = uart;
//   this->tx_pin_ = tx_pin;
//   this->setup_methods_.push_back(ONEWIRE_SETUP_UART_BUS_SINGLE_PIN);
// }

// void ESPHomeOneWireNGComponent::set_uart_bus(UARTComponent *uart) {
//   this->uart_ = uart;
//   this->setup_methods_.push_back(ONEWIRE_SETUP_UART_BUS);
// }

// Constructor definitions here
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent() {}
//  this->pin_config_ = OneWirePinConfig::SINGLE_PIN; }

ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(InternalGPIOPin *pin) {
  this->pin_ = pin;
  // Set bitbang mode
  //  this->pin_config_ = OneWirePinConfig::SINGLE_PIN;
}

ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(UARTComponent *uart) {
  this->uart_ = uart;
  // Set UART mode
  //  this->pin_config_ = OneWirePinConfig::UART;
}

void ESPHomeOneWireNGComponent::setup() {
  Component::setup();  // Call parent class setup()

  ESP_LOGCONFIG(TAG, "Setting up ESPHomeOneWireNGComponent...");

  // for (OneWireSetupMethod method : this->setup_methods_) {
  if (this->uart_ != nullptr) {
    uart_->setup();
    // Implement sending/receiving of data over UART
  } else if (this->pin_ != nullptr) {
    pin_->setup();
    // Bitbang mode setup
  }

  // switch (method) {
  // case ONEWIRE_SETUP_BITBANG_SINGLE_PIN:
  //   if (this->pin_ != nullptr) {
  //     pin_->setup();
  //     pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  //     delayMicroseconds(480);
  //     // Release the bus/pin
  //     delayMicroseconds(70);
  //   }
  //   break;
  // case ONEWIRE_SETUP_BITBANG_SPLIT_IO:
  //  ...
  //  }

  // Initialize 1-Wire bus and search for devices
  // ...
}

// void ESPHomeOneWireNGComponent::dump_config() {
//   ESP_LOGCONFIG(TAG, "Configuration:");
//
//   for (OneWireSetupMethod method : this->setup_methods_) {
//     switch (method) {
//       case ONEWIRE_SETUP_BITBANG_SINGLE_PIN:
//         ESP_LOGCONFIG(TAG, "  bitbang_single_pin");
//         if (this->pin_ != nullptr) {
//           ESP_LOGCONFIG(TAG, "  pin: %d", this->pin_->get_pin());
//         }
//         break;
//       // ...
//     }
//   }
// }

}  // namespace ow_bus_ng
}  // namespace esphome
