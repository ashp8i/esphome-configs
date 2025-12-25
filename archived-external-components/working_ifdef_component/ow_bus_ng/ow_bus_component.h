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

class ESPHomeOneWireNGComponent : public esphome::Component {
 public:
  ESPHomeOneWireNGComponent();

  ESPHomeOneWireNGComponent(InternalGPIOPin *pin);

  ESPHomeOneWireNGComponent(InputPin *input_pin, OutputPin *output_pin);

  // ESPHomeOneWireNGComponent(UARTComponent *uart, GPIOPin *tx_pin); // not yet supported, or ever

  ESPHomeOneWireNGComponent(UARTComponent *uart);

  void setup() override;
  bool perform_reset();

 protected:
  InternalGPIOPin *pin_{nullptr};
  InputPin *input_pin_{nullptr};
  OutputPin *output_pin_{nullptr};
  UARTComponent *uart_{nullptr};
  ESPHomeOneWireNGComponent *single_pin_bus_;  // Single pin mode
  ESPHomeOneWireNGComponent *split_io_bus_;    // Split IO mode
  ESPHomeOneWireNGComponent *uart_bus_;        // UART mode
};

}  // namespace ow_bus_ng
}  // namespace esphome
