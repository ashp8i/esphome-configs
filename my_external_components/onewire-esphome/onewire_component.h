#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/hal.h"

#include "OneWire_CurrentPlatform.h"
#include "drivers/DSTherm.h"

namespace esphome
{
  namespace onewire
  {

    class OnewireTemperatureSensor;

    class OnewireComponent : public PollingComponent
    {
    public:
      void set_pin(InternalGPIOPin *pin)
      {
        pin_ = pin;
        one_wire_ = new OneWire_CurrentPlatform(pin->get_pin(), false);
      }

      void setup() override;
      void dump_config() override;
      float get_setup_priority() const override { return setup_priority::DATA; }
      void register_sensor(OnewireTemperatureSensor *sensor)
      {
        sensors_.push_back(sensor);
      }

      void update() override;

    protected:
      friend OnewireTemperatureSensor;
      InternalGPIOPin *pin_;
      OneWire *one_wire_;
      std::vector<uint64_t> found_sensors_;
      std::vector<OneWireTemperatureSensor *> sensors_;
    };

  } // namespace onewire
} // namespace esphome

// onewire_bus.h

class OneWireBus {
  ...

  void setup() {
    one_wire_ = new OneWire(pin_);
    one_wire_->begin();
  }

  void update() {
    for (auto *device : devices_) {
      device->update();
    }   
  }

  std::vector<OneWireDevice*> devices_;

}

// onewire_device.h

class OneWireDevice {  
  void update() {
    ...
  }
}

// onewire_temperature_sensor.h

class OneWireTemperatureSensor : public OneWireDevice {
  float read_temperature() {
    ... // Read temperature using OneWire API
   }

  float get_temperature() {
    update();  // Call update() to fetch new data
    return read_temperature();
  }
}

// Register devices 
void OneWireBus::register_device(OneWireDevice *device) {
  devices_.push_back(device);
}
