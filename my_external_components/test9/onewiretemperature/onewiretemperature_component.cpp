#include "onewiretemperature_component.h"
#include "esphome/core/log.h"
#include <cstdint>

namespace esphome {
namespace onewiretemperature {

static const char *const TAG = "onewiretemperature";

// static const uint8_t DALLAS_MODEL_DS18S20 = 0x10;
// static const uint8_t DALLAS_MODEL_DS1822 = 0x22;
// static const uint8_t DALLAS_MODEL_DS18B20 = 0x28;
// static const uint8_t DALLAS_MODEL_DS1825 = 0x3B;
// static const uint8_t DALLAS_MODEL_DS28EA00 = 0x42;

// Constants
const uint8_t DALLAS_COMMAND_START_CONVERSION = 0x44;
const uint8_t DALLAS_COMMAND_READ_SCRATCH_PAD = 0xBE;
const uint8_t DALLAS_COMMAND_WRITE_SCRATCH_PAD = 0x4E;

void OneWireTemperature::setup() {

  bool success = this->read_scratch_pad();   
  if (!success) {
    this->status_set_error("Reading scratchpad failed");
    return; 
  }

  if (!this->check_scratch_pad()) {
    this->status_set_warning("Scratchpad failed checksum");
    return;
  }

  if (this->get_model() == DALLAS_MODEL_DS18S20) {
    // DS18S20 doesn't support resolution
    ESP_LOGW(TAG, "DS18S20 - Setting resolution skipped");   
  }
  else {
    uint8_t resolution = this->get_resolution();     
    this->set_scratch_pad(4, resolution);

    auto *wire = this->parent_->one_wire_;  
    
    if (wire->reset()) {
      // Write new resolution
    }
    else {
      this->status_set_warning("Failed to write resolution");
    }
  }  
}

float OneWireTemperature::get_setup_priority() const {
  return setup_priority::DATA;
}

void OneWireTemperature::update() {
  float temp = this->get_temp_c();  
  this->publish_state(temp);
}

void OneWireBusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "OneWireBus:");
  LOG_PIN("  Pin: ", this->pin_);
  
  for (auto *temperature : temperatures_) {
    ESP_LOGCONFIG(TAG, "  Temperature Sensor:");
    ESP_LOGCONFIG(TAG, "    Address: 0x%08X", temperature->get_address());  
    ESP_LOGCONFIG(TAG, "    Resolution: %uh", temperature->get_resolution());
    ESP_LOGCONFIG(TAG, "    Update Interval: %ums", temperature->get_update_interval());
    LOG_SENSOR("    ", "Sensor", temperature);
  }
}

sensor::State OneWireTemperature::get_state() {
  return this->state;  
}

std::string OneWireTemperature::unit_of_measurement() {
  return "°C";
}

bool OneWireTemperature::read_scratch_pad() {
  // Read from bus
  for (uint8_t i = 0; i < 9; i++) {  
    this->set_scratch_pad(i, wire->read8());
  }
  return true;
}

float OneWireTemperature::get_temp_c() {
  uint16_t msb = this->get_scratch_pad(1);
  uint16_t lsb = this->get_scratch_pad(0);
  // Convert to temperature
}

void OneWireBusComponent::register_temperature(OneWireTemperature *temperature) { 
  temperatures_.push_back(temperature);
}

enum class ScratchPadValidation {
  VALID,
  INVALID_CHECKSUM,
  INVALID_CONFIG 
};

ScratchPadValidation OneWireTemperature::check_scratch_pad() {
  
  bool chksum_validity = crc8(this->get_scratch_pad(), 8) == this->get_scratch_pad(8);
  
  if (!chksum_validity) {
    ESP_LOGW(TAG, "'%s' - Scratch pad checksum invalid!", this->get_name().c_str());
    return ScratchPadValidation::INVALID_CHECKSUM;  
  }
  
  bool config_validity = false;  
  switch (this->get_model()) {
    case DALLAS_MODEL_DS18B20:  
      config_validity = ...;    
      break;      
    default:         
      config_validity = ...;      
  }
  
  if (!config_validity) {
    ESP_LOGW(TAG, "'%s' - Invalid config register!", this->get_name().c_str());
    return ScratchPadValidation::INVALID_CONFIG; 
  }
  
  return ScratchPadValidation::VALID;
}

std::string OneWireTemperature::unique_id() {
  
  std::string model = this->get_model_name();  
  uint64_t address = this->get_address();    
    
  return model + "-" + std::to_string(address_hh) + std::to_string(address_lm);
}

}  // namespace onewiretemperature
}  // namespace esphome