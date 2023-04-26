#ifdef USE_BITBANG_SINGLE
#define USE_BITBANG_SINGLE
#endif

#ifdef USE_BITBANG_SPLIT_IO
#define USE_BITBANG_SPLIT_IO
#endif

#ifdef USE_UART_BUS
#define USE_UART_BUS
#endif

#include "ow_bus_component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ow_bus_ng {

static const char *const TAG = "owbus.ng";

// Constructor definitions here
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent() {}

#ifdef USE_BITBANG_SINGLE
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(InternalGPIOPin *pin) {
  if (pin->get_pin_mode() != OUTPUT_OPEN_DRAIN) {
    ESP_LOGE(TAG, "1-Wire pin %d must be in open-drain mode!", pin->get_pin());
    return;
  }
  this->pin_ = pin;
}
#endif

#ifdef USE_BITBANG_SPLIT_IO
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(InputPin *input_pin, OutputPin *output_pin) {
  this->input_pin_ = input_pin;
  this->output_pin_ = output_pin;
  // Set bitbang split IO mode
}
#endif

#ifdef USE_UART_BUS
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(UARTComponent *uart) {
  this->uart_ = uart;
  // Set UART mode
}
#endif

void ESPHomeOneWireNGComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESPHomeOneWireNGComponent...");
#ifdef USE_BITBANG_SINGLE
  ESP_LOGCONFIG(TAG, "Setting up bitbang single pin mode...");
  if (this->pin_ != nullptr) {
    if (!perform_reset())
      return;  // No device present
    single_pin_bus_ = new ESPHomeOneWireNGComponent(this->pin_->get_pin());
  }
#endif
#ifdef USE_BITBANG_SPLIT_IO
  ESP_LOGCONFIG(TAG, "Setting up bitbang split IO mode...");
  if (this->input_pin_ != nullptr && this->output_pin_ != nullptr) {
    if (!perform_reset())
      return;  // No device present
    split_io_bus_ = new ESPHomeOneWireNGComponent(this->input_pin_->get_pin(), this->output_pin_->get_pin());
  }
#endif
#ifdef USE_UART_BUS
  ESP_LOGCONFIG(TAG, "Setting up UART mode...");
  if (this->uart_ != nullptr) {
    if (!perform_reset())
      return;  // No device present
    uart_bus_ = new ESPHomeOneWireNGComponent(uart);
    uart_bus_->begin();
  }
#endif
}

void ESPHomeOneWireNGComponent::dump_config() {
#ifdef USE_BITBANG_SINGLE
  ESP_LOGCONFIG(TAG, "  Using bitbang single pin mode:");
  ESP_LOGCONFIG(TAG, "    Pin: %d", this->pin_->get_pin());
#endif
#ifdef USE_BITBANG_SPLIT_IO
  ESP_LOGCONFIG(TAG, "  Using bitbang split IO mode:");
  ESP_LOGCONFIG(TAG, "    Input pin: %d", this->input_pin_->get_pin());
  ESP_LOGCONFIG(TAG, "    Output pin: %d", this->output_pin_->get_pin());
#endif
#ifdef USE_UART_BUS
  ESP_LOGCONFIG(TAG, "  Using UART mode:");
  ESP_LOGCONFIG(TAG, "    UART bus: %s", this->uart_->get_name().c_str());
#endif
}

bool ESPHomeOneWireNGComponent::perform_reset() {
#ifdef USE_BITBANG_SINGLE
  if (this->pin_ != nullptr) {
    this->pin_->digital_write(LOW);
    delayMicroseconds(480);
    this->pin_->digital_write(HIGH);
    delayMicroseconds(70);
  }
#endif
#ifdef USE_BITBANG_SPLIT_IO
  if (this->input_pin_ != nullptr && this->output_pin_ != nullptr) {
    this->output_pin_->digital_write(LOW);
    delayMicroseconds(480);
    this->output_pin_->digital_write(HIGH);
     return this->input_pin_->digital_read() == LOW;
  }
#endif
#ifdef USE_UART_BUS
  if (this->uart_ != nullptr) {
    this->uart_->transmit_break();
    while (this->uart_->peek() == 0) { /* wait */ }
    return true;
  }
#endif
}

}  // namespace ow_bus_ng
}  // namespace esphome
