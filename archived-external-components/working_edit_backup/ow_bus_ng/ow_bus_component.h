#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h"
// #include "esphome/core/defines.h"
// #include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include <vector>

namespace esphome {
namespace ow_bus_ng {

using InputPin = InternalGPIOPin;
using OutputPin = InternalGPIOPin;

// enum OneWireSetupMethod {
//   ONEWIRE_SETUP_BITBANG_SINGLE_PIN,
//   ONEWIRE_SETUP_BITBANG_SPLIT_IO,
//   //  ONEWIRE_SETUP_UART_BUS_SINGLE_PIN, // No such thing, not supported by UART Component
//   ONEWIRE_SETUP_UART_BUS,
// };

class ESPHomeOneWireNGComponent : public esphome::Component {
 public:
  ESPHomeOneWireNGComponent();

  ESPHomeOneWireNGComponent(InternalGPIOPin *pin);

  ESPHomeOneWireNGComponent(InputPin *input_pin, OutputPin *output_pin);

  // ESPHomeOneWireNGComponent(UARTComponent *uart, GPIOPin *tx_pin); // not yet supported, or ever

  ESPHomeOneWireNGComponent(UARTComponent *uart);

  void set_bitbang_single_pin(InternalGPIOPin *pin);

  void set_bitbang_split_io(InternalGPIOPin *input_pin, InternalGPIOPin *output_pin);

  // void set_uart_single_pin(UARTComponent *uart, GPIOPin *tx_pin);

  void set_uart_bus(UARTComponent *uart);

  void setup() override;
  void dump_config() override;

 protected:
  InternalGPIOPin *pin_{nullptr};
  InternalGPIOPin *input_pin_{nullptr};
  InternalGPIOPin *output_pin_{nullptr};
  UARTComponent *uart_{nullptr};
  // std::vector<OneWireSetupMethod> setup_methods_;
};

}  // namespace ow_bus_ng
}  // namespace esphome
