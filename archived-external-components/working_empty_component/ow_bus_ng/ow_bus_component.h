#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h"
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

  void setup() override;

  void dump_config() override;
  enum OneWirePinConfig { SINGLE_PIN, SPLIT_IO };
  void set_single_pin(InputPin *pin);
  void set_split_io(OutputPin *tx_pin, InputPin *rx_pin);

 protected:
  InternalGPIOPin *pin_{nullptr};
  InternalGPIOPin *input_pin_{nullptr};
  InternalGPIOPin *output_pin_{nullptr};
  OneWirePinConfig pin_config_;
};

};  // namespace ow_bus_ng
};  // namespace esphome
