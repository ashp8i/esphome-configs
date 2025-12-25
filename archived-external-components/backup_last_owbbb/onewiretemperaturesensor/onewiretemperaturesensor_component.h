#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "onewirebus.h"

namespace esphome {
namespace onewiretemperaturesensor {

class OneWireTemperatureSensorComponent : public PollingComponent {
 public:
  void setup();
  void dump_config();
  void update();
  float get_state();

 protected: 
  std::string model_;
  std::string polling_interval_;
  uint32_t last_update_;
  float state;

};

}  // namespace ds18b20
}  // namespace esphome