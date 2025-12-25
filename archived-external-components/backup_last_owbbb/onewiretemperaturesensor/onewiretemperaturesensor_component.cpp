#include "onewiretemperaturesensor_component.h"
#include "esphome/core/log.h"
#include <cstdint>

namespace esphome {
namespace onewiretemperaturesensor {

static const char *const TAG = "onewiretemperaturesensor";

void OneWireTemperatureSensorComponent::setup() override {
  auto *bus = this->parent_->get_bus();
  auto success = this->read_scratch_pad(bus);
  
  if (!success) { 
    ESP_LOGW(TAG, "Could not read scratchpad for sensor at 0x%04X!", this->address_);
    return; 
  }
  
  auto family_code = this->get_address8()[0];
  if (family_code == 0x10) {
    this->model_ = "DS18S20"; 
  } else if (family_code == 0x28) {
    this->model_ = "DS18B20"; 
  }
  
  ESP_LOGCONFIG(TAG, "Found %s model sensor at 0x%04X", 
               this->model_.c_str(), this->address_);
               
  this->polling_interval_ = this->get_config().get<std::string>("polling_interval");
}

void OneWireTemperatureSensorComponent::dump_config() override {
  ESP_LOGCONFIG(TAG, "Found %s model sensor at 0x%04X", 
                this->model_.c_str(), this->address_);
  ESP_LOGCONFIG(TAG, "Resolution is %u bits", this->get_resolution());         
}  

void OneWireTemperatureSensorComponent::update() override {
  if (millis() - this->last_update_ > this->parse_duration(this->polling_interval_)) {
    auto *bus = this->parent_->get_bus();  
    auto success = this->read_scratch_pad(bus);
    if (!success) {
      ESP_LOGE(TAG, "Could not read scratchpad for sensor at 0x%04X!", this->address_);
      return; 
    } 

    float temperature;
    try {
      temperature = this->read_temperature(); 
    } catch (const owb_error& err) {
      ESP_LOGE(TAG, "Error reading sensor at 0x%04X: %s", this->address_, err.what());
    }
    this->state = sensor::state::State(temperature, "°C");
    this->last_update_ = millis();
  }
}

sensor::State OneWireTemperatureSensorComponent::get_state() override {
  return this->state;
}

}  // namespace onewiretemperaturesensor
}  // namespace esphome
