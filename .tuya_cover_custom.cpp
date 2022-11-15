#include "esphome/core/log.h"
#include "tuya_cover_custom.h"

namespace esphome {

const uint8_t COMMAND_OPEN = 0x00;
const uint8_t COMMAND_CLOSE = 0x02;
const uint8_t COMMAND_STOP = 0x01;

using namespace esphome::cover;

static const char *const TAG = "tuya.cover";

void TuyaCoverCustomCustom::setup() {
  this->value_range_ = this->max_value_ - this->min_value_;

  this->parent_->add_on_initialized_callback([this]() {
    // Set the direction (if configured/supported).
    this->set_direction_(this->invert_position_);

    // Handle configured restore mode.
    switch (this->restore_mode_) {
      case COVER_NO_RESTORE:
        break;
      case COVER_RESTORE: {
        auto restore = this->restore_state_();
        if (restore.has_value())
          restore->apply(this);
        break;
      }
      case COVER_RESTORE_AND_CALL: {
        auto restore = this->restore_state_();
        if (restore.has_value()) {
          restore->to_call(this).perform();
        }
        break;
      }
    }
  });

  uint8_t report_id = *this->position_id_;
  if (this->position_report_id_.has_value()) {
    // A position report datapoint is configured; listen to that instead.
    report_id = *this->position_report_id_;
  }

  this->parent_->register_listener(report_id, [this](const TuyaDatapoint &datapoint) {
    if (datapoint.value_int == 123) {
      ESP_LOGD(TAG, "Ignoring MCU position report - not calibrated");
      return;
    }
    auto pos = float(datapoint.value_uint - this->min_value_) / this->value_range_;
    this->position = 1.0f - pos;
    this->publish_state();
  });
}

void TuyaCoverCustomCustom::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    if (this->control_id_.has_value()) {
      this->parent_->force_set_enum_datapoint_value(*this->control_id_, COMMAND_STOP);
    } else {
      auto pos = this->position;
      pos = 1.0f - pos;
      auto position_int = static_cast<uint32_t>(pos * this->value_range_);
      position_int = position_int + this->min_value_;

      parent_->force_set_integer_datapoint_value(*this->position_id_, position_int);
    }
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (this->control_id_.has_value() && (pos == COVER_OPEN || pos == COVER_CLOSED)) {
      if (pos == COVER_OPEN) {
        this->parent_->force_set_enum_datapoint_value(*this->control_id_, COMMAND_OPEN);
      } else {
        this->parent_->force_set_enum_datapoint_value(*this->control_id_, COMMAND_CLOSE);
      }
    } else {
      pos = 1.0f - pos;
      auto position_int = static_cast<uint32_t>(pos * this->value_range_);
      position_int = position_int + this->min_value_;

      parent_->force_set_integer_datapoint_value(*this->position_id_, position_int);
    }
  }

  this->publish_state();
}

void TuyaCoverCustomCustom::set_direction_(bool inverted) {
  if (!this->direction_id_.has_value()) {
    return;
  }

  if (inverted) {
    ESP_LOGD(TAG, "Setting direction: inverted");
  } else {
    ESP_LOGD(TAG, "Setting direction: normal");
  }

  this->parent_->set_boolean_datapoint_value(*this->direction_id_, inverted);
}

void TuyaCoverCustomCustom::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Cover:");
  if (this->invert_position_) {
    if (this->direction_id_.has_value()) {
      ESP_LOGCONFIG(TAG, "   Inverted");
    } else {
      ESP_LOGCONFIG(TAG, "   Configured as Inverted, but direction_datapoint isn't configured");
    }
  }
  if (this->control_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Control has datapoint ID %u", *this->control_id_);
  if (this->direction_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Direction has datapoint ID %u", *this->direction_id_);
  if (this->position_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Position has datapoint ID %u", *this->position_id_);
  if (this->position_report_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Position Report has datapoint ID %u", *this->position_report_id_);
}

cover::CoverTraits TuyaCoverCustomCustom::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_position(true);
  return traits;
}

}  // namespace esphome

//template code

#include "esphome.h"
#include "esphome/core/helpers.h"

static const char *const TAG = "electriq_ac";

uint8_t ac_mode = 0x03;
uint8_t fan_speed = 0x90;
uint8_t swing = 0;
uint8_t target_temp = 0;

class TuyaCoverCustomCustomCustom : public Component, public UARTDevice, public Cover
{
public:
  TuyaCoverCustomCustomCustom(UARTComponent *parent) : UARTDevice(parent) {}

  void setup() override
  {
    this->set_interval("heartbeat", 1800, [this]{ SendHeartbeat(); });
  }

  // calculate checksum and write out the serial message
  void SendToMCU()
  {
    uint8_t tuyacmd;
    tuyacmd = (ac_mode + fan_speed);
    // ensure we have obtained the MCU settings and published before commanding the MCU
    if (target_temp != 0)
    {
      uint8_t checksum = (0xAA + 0x03 + tuyacmd + swing + target_temp + 0x0B);
      write_array({0xAA, 0x03, tuyacmd, swing, target_temp, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, checksum});
      ESP_LOGD(TAG, "SendToMCU fan/mode: %s, target_temp: %s", format_hex_pretty(tuyacmd).c_str(), format_hex_pretty(target_temp).c_str());
      // we wrote something, so ensure it's published back to HA too
      this->publish_state();
    }
    else
    {
      ESP_LOGD(TAG, "Something to write but target_temp zero? %s", format_hex_pretty(target_temp).c_str());
    }
  }

  // send regular heartbeat and check for any response
  // any response we read here is likely to be from the previous heartbeat. Not a huge deal to wait 1.6 seconds
  void SendHeartbeat()
  {
    write_array({0xAA, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAC});
    ReadMCU();
  }

  // Select command nibble for fan speed
  void AcFanSpeed()
  {
    switch (this->fan_mode.value())
    {
    case climate::CLIMATE_FAN_LOW:
    default:
      fan_speed = 0x90;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = 0xB0;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = 0xD0;
      break;
    }
  }

  // Select command nibble for mode
  void AcModes()
  {
    switch (this->mode)
    {
    case climate::CLIMATE_MODE_COOL:
      ac_mode = 0x01;
      break;
    case climate::CLIMATE_MODE_DRY:
      ac_mode = 0x02;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      ac_mode = 0x03;
      break;
    case climate::CLIMATE_MODE_HEAT:
      ac_mode = 0x04;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      fan_speed = 0x10;
      break;
    }
  }

  // Select command nibble for swing
  void AcSwing()
  {
    switch (this->swing_mode)
    {
    case climate::CLIMATE_SWING_OFF:
    default:
      swing = 0x00;
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      swing = 0x0C;
      break;
    }
  }

  bool CheckIdle(uint8_t &a)
  {
    if (a == 0x00)
    {
      return true;
    }
    return false;
  }

  // read and parse messages from MCU serial
  void ReadMCU()
  {
    uint8_t pos = 0;
    uint8_t csum = 0;
    uint8_t c;
    uint8_t b[16];
    // find header byte, read further 16 bytes into array
    while (this->available())
    {
      read_byte(&c);
      if (c == 0xAA)
      {
        read_array(b, 16);
        // if any more bytes available in the serial buffer, read each one to clear them out
        while (this->available())
        {
          read_byte(&c);
        }
        // validate the checksum before progressing
        while (pos < 14)
        {
          csum += b[pos];
          ++pos;
        }
        if ((csum += 0xAA) != b[15])
        {
          ESP_LOGD(TAG, "Bad received checksum %s, should be %s", format_hex_pretty(csum).c_str(), format_hex_pretty(b[15]).c_str());
          return;
        }
        // Simple bitwise AND ops to get fan, mode, swing and action nibbles
        uint8_t f = (b[1] & 0xF0);
        uint8_t m = (b[1] & 0x0F);
        uint8_t s = (b[2] & 0x0F);
        uint8_t a = (b[11] & 0x0F);
        static uint8_t last_b1;  // command
        static uint8_t last_b2;  // swing
        static uint8_t last_b3;  // set temp
        static uint8_t last_b7;  // temp probe
        static uint8_t last_b11; // active state

        if (f == 0x10)
        {
          ESP_LOGD(TAG, "Detected mode: Standby");
          this->action = climate::CLIMATE_ACTION_OFF;
          this->mode = climate::CLIMATE_MODE_OFF;
          AcModes();
        }
        else
        { // not in standby, report mode / idle at all times
          switch (m)
          {
          case 0x01:
          default:
            if (!CheckIdle(a))
            {
              this->action = climate::CLIMATE_ACTION_COOLING;
            }
            ESP_LOGD(TAG, "Detected mode: Cool");
            this->mode = climate::CLIMATE_MODE_COOL;
            AcModes();
            break;
          case 0x02:
            if (!CheckIdle(a))
            {
              this->action = climate::CLIMATE_ACTION_DRYING;
            }
            ESP_LOGD(TAG, "Detected mode: Dry");
            this->mode = climate::CLIMATE_MODE_DRY;
            AcModes();
            break;
          case 0x03:
            if (!CheckIdle(a))
            {
              this->action = climate::CLIMATE_ACTION_FAN;
            }
            ESP_LOGD(TAG, "Detected mode: Fan");
            this->mode = climate::CLIMATE_MODE_FAN_ONLY;
            AcModes();
            break;
          case 0x04:
            if (!CheckIdle(a))
            {
              this->action = climate::CLIMATE_ACTION_HEATING;
            }
            ESP_LOGD(TAG, "Detected mode: Heat");
            this->mode = climate::CLIMATE_MODE_HEAT;
            AcModes();
            break;
          }
          if (CheckIdle(a))
          {
            ESP_LOGD(TAG, "Detected action: Idle");
            this->action = climate::CLIMATE_ACTION_IDLE;
          }
        }
        // update fan speed
        switch (f)
        {
        case 0x90:
        default:
          ESP_LOGD(TAG, "Detected fan: low");
          this->fan_mode = climate::CLIMATE_FAN_LOW;
          break;
        case 0xB0:
          ESP_LOGD(TAG, "Detected fan: medium");
          this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
          break;
        case 0xD0:
          ESP_LOGD(TAG, "Detected fan: high");
          this->fan_mode = climate::CLIMATE_FAN_HIGH;
          break;
        }
        // update swing
        if (s == 0x0C)
        {
          ESP_LOGD(TAG, "Detected swing: on");
          this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        }
        else
        {
          ESP_LOGD(TAG, "Detected swing: off");
          this->swing_mode = climate::CLIMATE_SWING_OFF;
        }

        this->current_temperature = b[7];
        this->target_temperature = b[3];
        target_temp = b[3];

        // only publish state if something changes
        if ((last_b1 != b[1]) || (last_b2 != b[2]) || (last_b3 != b[3]) || (last_b7 != b[7]) || (last_b11 != b[11]))
        {
          ESP_LOGD(TAG, "Publishing new state...");
          this->publish_state();
          last_b1 = b[1];
          last_b2 = b[2];
          last_b3 = b[3];
          last_b7 = b[7];
          last_b11 = b[11];
        }
      }
    }
  }

  void control(const ClimateCall &call) override
  {
    if (call.get_mode().has_value())
    {
      ESP_LOGD(TAG, "New mode value seen");
      ClimateMode mode = *call.get_mode();
      this->mode = mode;
      AcModes();
      // if the mode isn't standby (denoted by 0x10 fan speed), set the fan speed
      if (this->mode != climate::CLIMATE_MODE_OFF)
      {
        AcFanSpeed();
      }
      SendToMCU();
    }
    else if (call.get_target_temperature().has_value())
    {
      target_temp = *call.get_target_temperature();
      // Set fan speed nibble here to avoid unexpected switch-off on temp changes
      AcFanSpeed();
      SendToMCU();
    }
    else if (call.get_fan_mode().has_value())
    {
      ClimateFanMode fan_mode = *call.get_fan_mode();
      this->fan_mode = fan_mode;
      AcFanSpeed();
      SendToMCU();
    }
    else if (call.get_swing_mode().has_value())
    {
      ClimateSwingMode swing_mode = *call.get_swing_mode();
      this->swing_mode = swing_mode;
      AcSwing();
      // Set fan speed nibble here to avoid unexpected switch-off on temp changes
      AcFanSpeed();
      SendToMCU();
    }
  }

  ClimateTraits traits() override
  {
    auto traits = climate::ClimateTraits();
    traits.set_supports_action(true);
    traits.set_supports_two_point_target_temperature(false);
    traits.set_supports_current_temperature(true);
    traits.set_visual_min_temperature(16);
    traits.set_visual_max_temperature(32);
    traits.set_visual_temperature_step(1);

    traits.set_supported_modes({climate::CLIMATE_MODE_OFF,
                                climate::CLIMATE_MODE_COOL,
                                climate::CLIMATE_MODE_HEAT,
                                climate::CLIMATE_MODE_DRY,
                                climate::CLIMATE_MODE_FAN_ONLY});

    traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF,
                                      climate::CLIMATE_SWING_VERTICAL});

    traits.set_supported_fan_modes({climate::CLIMATE_FAN_LOW,
                                    climate::CLIMATE_FAN_MEDIUM,
                                    climate::CLIMATE_FAN_HIGH});

    return traits;
  }
};