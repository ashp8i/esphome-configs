#include "DS28E17.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ds28e17 {

static const char *const TAG = "DS28E17";

i2c::ErrorCode DS28E17Channel::readv(uint8_t address, i2c::ReadBuffer *buffers, size_t cnt) {
  auto err = parent_->switch_to_channel(channel_);
  if (err != i2c::ERROR_OK)
    return err;
  return parent_->bus_->readv(address, buffers, cnt);
}
i2c::ErrorCode DS28E17Channel::writev(uint8_t address, i2c::WriteBuffer *buffers, size_t cnt, bool stop) {
  auto err = parent_->switch_to_channel(channel_);
  if (err != i2c::ERROR_OK)
    return err;
  return parent_->bus_->writev(address, buffers, cnt, stop);
}

void DS28E17Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS28E17...");
  uint8_t status = 0;
  if (this->read(&status, 1) != i2c::ERROR_OK) {
    ESP_LOGI(TAG, "DS28E17 failed");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Channels currently open: %d", status);
}
void DS28E17Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS28E17:");
  LOG_I2C_DEVICE(this);
}

i2c::ErrorCode DS28E17Component::switch_to_channel(uint8_t channel) {
  if (this->is_failed())
    return i2c::ERROR_NOT_INITIALIZED;
  if (current_channel_ == channel)
    return i2c::ERROR_OK;

  uint8_t channel_val = 1 << channel;
  auto err = this->write(&channel_val, 1);
  if (err == i2c::ERROR_OK) {
    current_channel_ = channel;
  }
  return err;
}

}  // namespace ds28e17
}  // namespace esphome
