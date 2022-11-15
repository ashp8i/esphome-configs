#pragma once

#include "esphome/core/component.h"
#include "esphome/components/cover/cover.h"

namespace esphome {

// enum TuyaCoverCustomRestoreMode {
//   COVER_NO_RESTORE,
//   COVER_RESTORE,
//   COVER_RESTORE_AND_CALL,
// };

class TuyaCoverCustom : public cover::Cover, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_control_id(uint8_t control_id) { this->control_id_ = control_id; }
  void set_direction_id(uint8_t direction_id) { this->direction_id_ = direction_id; }
  void set_position_id(uint8_t position_id) { this->position_id_ = position_id; }
  void set_position_report_id(uint8_t position_report_id) { this->position_report_id_ = position_report_id; }
  void set_tuya_parent(Tuya *parent) { this->parent_ = parent; }
  void set_min_value(uint32_t min_value) { min_value_ = min_value; }
  void set_max_value(uint32_t max_value) { max_value_ = max_value; }
  void set_invert_position(bool invert_position) { invert_position_ = invert_position; }
  // void set_restore_mode(TuyaCoverCustomRestoreMode restore_mode) { restore_mode_ = restore_mode; }
  TuyaCoverCustomCustom(UARTComponent *parent) : UARTDevice(parent) {}

  void setup() override
  {
    this->set_interval("heartbeat", 1800, [this]{ SendHeartbeat(); });
  }

  // calculate checksum and write out the serial message
  void SendToMCU()
  {
    uint8_t Command

 protected:
  void control(const cover::CoverCall &call) override;
  void set_direction_(bool inverted);
  cover::CoverTraits get_traits() override;

  // Tuya *parent_;
  // TuyaCoverCustomRestoreMode restore_mode_{};
  optional<uint8_t> control_id_{};
  optional<uint8_t> direction_id_{};
  optional<uint8_t> position_id_{};
  optional<uint8_t> position_report_id_{};
  uint32_t min_value_;
  uint32_t max_value_;
  uint32_t value_range_;
  bool invert_position_;
};



}  // namespace esphome