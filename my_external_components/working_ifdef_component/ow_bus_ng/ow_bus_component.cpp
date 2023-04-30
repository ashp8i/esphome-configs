#ifdef USE_SINGLE_PIN_BIT_BANG
#define USE_SINGLE_PIN_BIT_BANG
#endif

#ifdef USE_SPLIT_IO_BIT_BANG
#define USE_SPLIT_IO_BIT_BANG
#endif

#ifdef USE_SINGLE_PIN_RMT
#define USE_SINGLE_PIN_RMT
#endif

#ifdef USE_SPLIT_IO_RMT
#define USE_SPLIT_IO_RMT
#endif

#include "ow_bus_component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ow_bus_ng {

static const char *const TAG = "owbus.ng";

// Constructor definitions here
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent() {}

#ifdef USE_SINGLE_PIN_BIT_BANG
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(InternalGPIOPin *pin) {
  this->pin_ = pin;
}
#endif

#ifdef USE_SPLIT_IO_BIT_BANG
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(InputPin *input_pin, OutputPin *output_pin) {
  this->input_pin_ = input_pin;
  this->output_pin_ = output_pin;
  // Set bitbang split IO mode
}
#endif

#ifdef USE_SINGLE_PIN_RMT
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(InternalGPIOPin *pin) {
    // Configure RMT TX channel
    rmt_config_t c{};
    c.channel = RMT_CHANNEL_0;
    c.gpio_num = this->pin_->get_pin();
    c.clk_div = 80;
    c.mem_block_num = 1;
    c.tx_config.loop_en = false;
    c.tx_config.carrier_en = false;
    c.tx_config.idle_level = 1;
    c.tx_config.idle_output_en = true;
    this->config_rmt(c);
    c.rmt_mode = RMT_MODE_TX;
    rmt_driver_install(c.channel, &c, 0);

    // Set RMT RX channel to use the same GPIO pin
    rmt_set_gpio(RMT_CHANNEL_1, RMT_MODE_RX, this->pin_->get_pin(), 0);

    // Enable input on GPIO pin 
    this->pin_->setup_input(HAS_PULL_UP);  
  // Set UART mode
}
#endif

#ifdef USE_SPLIT_IO_RMT
ESPHomeOneWireNGComponent::ESPHomeOneWireNGComponent(InputPin *input_pin, OutputPin *output_pin) {
  // Need to write/define/tweak
}
#endif

void ESPHomeOneWireNGComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESPHomeOneWireNGComponent...");
#ifdef USE_SINGLE_PIN_BIT_BANG
  ESP_LOGCONFIG(TAG, "Setting up single pin bit bang mode...");
  if (this->pin_ != nullptr) {
    if (!perform_reset())
      return;  // No device present
    single_pin_bus_ = new ESPHomeOneWireNGComponent(this->pin_->get_pin());
  }
#endif
#ifdef USE_SPLIT_IO_BIT_BANG
  ESP_LOGCONFIG(TAG, "Setting up split IO bit bang mode...");
  if (this->input_pin_ != nullptr && this->output_pin_ != nullptr) {
    if (!perform_reset())
      return;  // No device present
    split_io_bus_ = new ESPHomeOneWireNGComponent(this->input_pin_->get_pin(), this->output_pin_->get_pin());
  }
#endif
#ifdef USE_SINGLE_PIN_RMT
    ESP_LOGCONFIG(TAG, "Setting up single pin RMT mode...");
  if (this->pin_ != nullptr) {
    if (!perform_reset())
      return;  // No device present
  //to define
  }
#endif
#ifdef USE_SPLIT_IO_RMT
  ESP_LOGCONFIG(TAG, "Setting up split io RMT mode...");
  if (this->pin_ != nullptr) {
    if (!perform_reset())
      return;  // No device present
  //to define
  }
#endif
}

void ESPHomeOneWireNGComponent::dump_config() {
#ifdef USE_SINGLE_PIN_BIT_BANG
  ESP_LOGCONFIG(TAG, "  Using single pin bit bang mode:");
  ESP_LOGCONFIG(TAG, "    Pin: %d", this->pin_->get_pin());
#endif
#ifdef USE_SPLIT_IO_BIT_BANG
  ESP_LOGCONFIG(TAG, "  Using split io bit bang mode:");
  ESP_LOGCONFIG(TAG, "    Input pin: %d", this->input_pin_->get_pin());
  ESP_LOGCONFIG(TAG, "    Output pin: %d", this->output_pin_->get_pin());
#endif
#ifdef USE_SINGLE_PIN_RMT
  ESP_LOGCONFIG(TAG, "  Using single pin RMT mode:");
  ESP_LOGCONFIG(TAG, "    Pin: %d", this->pin_->get_pin());
#endif
#ifdef USE_SPLIT_IO_RMT
  ESP_LOGCONFIG(TAG, "  Using split io RMT mode:");
  ESP_LOGCONFIG(TAG, "    Input pin: %d", this->input_pin_->get_pin());
  ESP_LOGCONFIG(TAG, "    Output pin: %d", this->output_pin_->get_pin());
#endif
}

bool ESPHomeOneWireNGComponent::perform_reset() {
#ifdef USE_SINGLE_PIN_BIT_BANG
  if (this->pin_ != nullptr) {
    this->pin_->digital_write(false);
    delayMicroseconds(480);
    this->pin_->digital_write(true);
    delayMicroseconds(70);
  }
#endif
#ifdef USE_SPLIT_IO_BIT_BANG
  if (this->input_pin_ != nullptr && this->output_pin_ != nullptr) {
    this->output_pin_->digital_write(false);
    delayMicroseconds(480);
    this->output_pin_->digital_write(true);
     return this->input_pin_->digital_read() == LOW;
  }
#endif
#ifdef USE_SINGLE_PIN_RMT
  if (this->pin_ != nullptr) {
    this->pin_->digital_write(false);
    delayMicroseconds(480);
    this->pin_->digital_write(true);
    delayMicroseconds(70);
  }
#endif
#ifdef USE_SPLIT_IO_RMT
  if (this->input_pin_ != nullptr && this->output_pin_ != nullptr) {
    this->output_pin_->digital_write(false);
    delayMicroseconds(480);
    this->output_pin_->digital_write(true);
     return this->input_pin_->digital_read() == LOW;
  }
#endif
}

}  // namespace ow_bus_ng
}  // namespace esphome
