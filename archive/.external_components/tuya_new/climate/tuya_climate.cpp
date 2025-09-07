#include "esphome/core/log.h"
#include "tuya_climate.h"

namespace esphome {
namespace tuya_new {

static const char *TAG = "tuya.climate";

void TuyaClimate::setup() {
  if (this->switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, TuyaDatapointType::BOOLEAN, [this](TuyaDatapoint datapoint) {
      ESP_LOGD(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
      this->mode = climate::CLIMATE_MODE_OFF;
      if (datapoint.value_bool) {
        if (this->supports_heat_ && this->supports_cool_) {
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        } else if (this->supports_heat_) {
          this->mode = climate::CLIMATE_MODE_HEAT;
        } else if (this->supports_cool_) {
          this->mode = climate::CLIMATE_MODE_COOL;
        }
      }
      this->compute_state_();
      this->publish_state();
    });
  }
  if (this->target_temperature_id_.has_value()) {
    this->parent_->register_listener(
        *this->target_temperature_id_, TuyaDatapointType::INTEGER, [this](TuyaDatapoint datapoint) {
          this->target_temperature = datapoint.value_int * this->target_temperature_multiplier_;
          ESP_LOGD(TAG, "MCU reported target temperature is: %.1f", this->target_temperature);
          this->compute_state_();
          this->publish_state();
        });
  }
  if (this->current_temperature_id_.has_value()) {
    this->parent_->register_listener(
        *this->current_temperature_id_, TuyaDatapointType::INTEGER, [this](TuyaDatapoint datapoint) {
          this->current_temperature = datapoint.value_int * this->current_temperature_multiplier_;
          ESP_LOGD(TAG, "MCU reported current temperature is: %.1f", this->current_temperature);
          this->compute_state_();
          this->publish_state();
        });
  }
}

void TuyaClimate::control(const climate::ClimateCall &call) {
  std::vector<uint8_t> data;
  data.resize(15);
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
    data[*this->switch_id_] = this->mode == climate::CLIMATE_MODE_OFF ? 0x02 : 0x01;
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
    data[*this->target_temperature_id_] = (int) (this->target_temperature / this->target_temperature_multiplier_);
  }

  this->parent_->set_datapoint_value(data);
}

climate::ClimateTraits TuyaClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->current_temperature_id_.has_value());
  std::set<climate::ClimateMode> modes = {climate::CLIMATE_MODE_OFF};
  if (this->supports_heat_) {
    modes.insert(climate::CLIMATE_MODE_HEAT);
  }
  if (this->supports_cool_) {
    modes.insert(climate::CLIMATE_MODE_COOL);
  }
  traits.set_supported_modes(modes);
  traits.set_supports_action(true);
  return traits;
}

void TuyaClimate::dump_config() {
  LOG_CLIMATE("", "Tuya Climate", this);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
  if (this->target_temperature_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Target Temperature has datapoint ID %u", *this->target_temperature_id_);
  if (this->current_temperature_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Current Temperature has datapoint ID %u", *this->current_temperature_id_);
}

void TuyaClimate::compute_state_() {
  if (std::isnan(this->current_temperature) || std::isnan(this->target_temperature)) {
    // if any control parameters are nan, go to OFF action (not IDLE!)
    this->switch_to_action_(climate::CLIMATE_ACTION_OFF);
    return;
  }

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->switch_to_action_(climate::CLIMATE_ACTION_OFF);
    return;
  }

  climate::ClimateAction target_action = climate::CLIMATE_ACTION_IDLE;
  const float temp_diff = this->target_temperature + this->hysteresis_ - this->current_temperature;
  if (std::abs(temp_diff) > this->hysteresis_) {
    if (this->supports_heat_ && temp_diff > 0) {
      target_action = climate::CLIMATE_ACTION_HEATING;
    } else if (this->supports_cool_ && temp_diff < 0) {
      target_action = climate::CLIMATE_ACTION_COOLING;
    }
  }
  this->switch_to_action_(target_action);
}

void TuyaClimate::switch_to_action_(climate::ClimateAction action) {
  // For now this just sets the current action but could include triggers later
  this->action = action;
}

}  // namespace tuya_new
}  // namespace esphome

/**
 *
 *  18 + 2 - 18
 *  2 = heating
 *
 *  18 + 2 - 19
 *  1 = heating
 *
 *
 *
 **/
