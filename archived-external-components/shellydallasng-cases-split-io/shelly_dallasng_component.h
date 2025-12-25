#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/hal.h"

#include "OneWireNg_CurrentPlatform.h"
#include "drivers/DSTherm.h"

namespace esphome
{
  namespace shellydallasng
  {

    class ShellyDallasNgTemperatureSensor;

    class ShellyDallasNgComponent : public PollingComponent
    {
    public:
      void set_pin(InternalGPIOPin *pin)
      {
        pin_ = pin;
        one_wire_ = new OneWireNg_CurrentPlatform(pin->get_pin(), false);
      }
      void set_pins(InternalGPIOPin *input_pin, InternalGPIOPin *output_pin)
      {
        input_pin_ = input_pin;
        output_pin_ = output_pin;
        one_wire_input_ = new OneWireNg_CurrentPlatform(input_pin_->get_pin(), false);   
        one_wire_output_ = new OneWireNg_CurrentPlatform(output_pin_->get_pin(), false);
      }

      void setup() override;
      void dump_config() override;
      float get_setup_priority() const override { return setup_priority::DATA; }
      void register_sensor(ShellyDallasNgTemperatureSensor *sensor)
      {
        sensors_.push_back(sensor);
      }

      void update() override;

// Old implementation, replaced for encapsulation 
/*
    protected:
      friend ShellyDallasNgTemperatureSensor;
*/

    private:
      InternalGPIOPin *pin_; 
      InternalGPIOPin *input_pin_; 
      InternalGPIOPin *output_pin_;
      OneWireNg *one_wire_;
      OneWireNg *one_wire_input_;
      OneWireNg *one_wire_output_;
      std::vector<uint64_t> found_sensors_;
      std::vector<ShellyDallasNgTemperatureSensor *> sensors_;
    };

  } // namespace shellydallasng
} // namespace esphome
