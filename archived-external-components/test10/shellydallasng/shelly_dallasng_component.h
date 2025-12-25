#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/hal.h"

#include "OneWireNg_CurrentPlatform.h"
#include "drivers/DSTherm.h"

class GPIOPin {
 public:
  GPIOPin(int pin) : pin_(pin) {}
  int get_pin() const { return pin_; }
 private:
  int pin_;
};

namespace esphome
{
  namespace shellydallasng
  {

    class ShellyDallasNgTemperatureSensor;

    class ShellyDallasNgComponent : public PollingComponent
    {
    public:
      void set_pins(int input_pin, int output_pin) { 
        input_pin_ = input_pin;
        output_pin_ = output_pin;
        one_wire_input_ = new OneWireNg_CurrentPlatform(input_pin, false);   
        one_wire_output_ = new OneWireNg_CurrentPlatform(output_pin, false); 
      }

      void setup() override;
      void dump_config() override;
      float get_setup_priority() const override { return setup_priority::DATA; }
      void register_sensor(ShellyDallasNgTemperatureSensor *sensor)
      {
        sensors_.push_back(sensor);
      }

      void update() override;

    protected:
      friend ShellyDallasNgTemperatureSensor;

    private:
      int input_pin_; 
      int output_pin_;
      OneWireNg *one_wire_input_;
      OneWireNg *one_wire_output_;
      std::vector<uint64_t> found_sensors_;
      std::vector<ShellyDallasNgTemperatureSensor *> sensors_;
    };

  } // namespace shellydallasng
} // namespace esphome
