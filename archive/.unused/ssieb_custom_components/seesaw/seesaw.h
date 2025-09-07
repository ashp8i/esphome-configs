#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace seesaw {

enum SeesawModule : uint8_t {
  SEESAW_STATUS = 0x00,
  SEESAW_GPIO = 0x01,
  SEESAW_SERCOM0 = 0x02,
  SEESAW_TIMER = 0x08,
  SEESAW_ADC = 0x09,
  SEESAW_DAC = 0x0A,
  SEESAW_INTERRUPT = 0x0B,
  SEESAW_DAP = 0x0C,
  SEESAW_EEPROM = 0x0D,
  SEESAW_NEOPIXEL = 0x0E,
  SEESAW_TOUCH = 0x0F,
  SEESAW_KEYPAD = 0x10,
  SEESAW_ENCODER = 0x11,
};

enum : uint8_t {
  SEESAW_STATUS_HW_ID = 0x01,
  SEESAW_STATUS_VERSION = 0x02,
  SEESAW_STATUS_OPTIONS = 0x03,
  SEESAW_STATUS_TEMP = 0x04,
  SEESAW_STATUS_SWRST = 0x7F,
};

enum : uint8_t {
  SEESAW_ENCODER_STATUS = 0x00,
  SEESAW_ENCODER_INTENSET = 0x10,
  SEESAW_ENCODER_INTENCLR = 0x20,
  SEESAW_ENCODER_POSITION = 0x30,
  SEESAW_ENCODER_DELTA = 0x40,
};

enum : uint8_t {
  SEESAW_GPIO_DIRSET_BULK = 0x02,
  SEESAW_GPIO_DIRCLR_BULK = 0x03,
  SEESAW_GPIO_BULK = 0x04,
  SEESAW_GPIO_BULK_SET = 0x05,
  SEESAW_GPIO_BULK_CLR = 0x06,
  SEESAW_GPIO_BULK_TOGGLE = 0x07,
  SEESAW_GPIO_INTENSET = 0x08,
  SEESAW_GPIO_INTENCLR = 0x09,
  SEESAW_GPIO_INTFLAG = 0x0A,
  SEESAW_GPIO_PULLENSET = 0x0B,
  SEESAW_GPIO_PULLENCLR = 0x0C,
};

enum : uint8_t {
  SEESAW_NEOPIXEL_STATUS = 0x00,
  SEESAW_NEOPIXEL_PIN = 0x01,
  SEESAW_NEOPIXEL_SPEED = 0x02,
  SEESAW_NEOPIXEL_BUF_LENGTH = 0x03,
  SEESAW_NEOPIXEL_BUF = 0x04,
  SEESAW_NEOPIXEL_SHOW = 0x05,
};

enum : uint8_t {
  SEESAW_TOUCH_CHANNEL_OFFSET = 0x10,
};


class Seesaw : public i2c::I2CDevice, public Component {
 public:
  void setup() override;

  float get_setup_priority() const override;

  void enable_encoder(uint8_t number);
  int32_t get_encoder_position(uint8_t number);
  int16_t get_touch_value(uint8_t channel);
  float get_temperature();
  void set_pinmode(uint8_t pin, uint8_t mode);
  bool digital_read(uint8_t pin);
  void set_gpio_interrupt(uint32_t pin, bool enabled);
  void setup_neopixel();
  void color_neopixel(uint8_t r, uint8_t g, uint8_t b);

 protected:
  i2c::ErrorCode write8(SeesawModule mod, uint8_t reg, uint8_t value);
  i2c::ErrorCode write16(SeesawModule mod, uint8_t reg, uint16_t value);
  i2c::ErrorCode write32(SeesawModule mod, uint8_t reg, uint32_t value);
  i2c::ErrorCode readbuf(SeesawModule mod, uint8_t reg, uint8_t *buf, uint8_t len);
};

/*
class SeesawGPIOPin : public GPIOPin {
 public:
  void setup() override;
  void pin_mode(gpio::Flags flags) override;
  bool digital_read() override;
  void digital_write(bool value) override;
  std::string dump_summary() const override;

  void set_parent(Seesaw *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }
  void set_interrupt_mode(MCP23XXXInterruptMode interrupt_mode) { interrupt_mode_ = interrupt_mode; }

 protected:
  Seesaw *parent_;
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};
*/

}  // namespace seesaw
}  // namespace esphome
