#include "seesawtouch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace seesaw {

static const char *const TAG = "seesaw.touch";

void SeesawTouch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Seesaw touch sensor...");
}

void SeesawTouch::update() {
  uint16_t value = this->parent_->get_touch_value(this->channel_);
  if (value == -1)
    ESP_LOGW(TAG, "touch reading failed for channel %d", this->channel_);
  else
    this->publish_state(value);
}

}  // namespace seesaw
}  // namespace esphome
