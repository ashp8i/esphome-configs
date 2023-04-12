#pragma once

#include <cstdint> // Include for uint8_t, uint16_t, and uint64_t
#include <cstddef> // Include for size_t
#include "esphome/core/component.h"
#include "esphome/core/helpers.h" // Include for esphome::ErrorCode
#include "esphome/components/i2c/i2c.h"
#include "custom_onewire.h" // Include custom_onewire.h instead of esphome/components/onewire/onewire.h

#define ONEWIRE_TIMEOUT 50

#define DS28E17_ENABLE_SLEEP 0x1E
#define DS28E17_WRITE 0x4B
#define DS28E17_READ 0x87
#define DS28E17_MEMORY_READ 0x2D

namespace ds28e17_i2c_bus {

// Custom buffer classes for DS28E17I2CBus component
class DS28E17I2CBusReadBuffer {
public:
  DS28E17I2CBusReadBuffer(uint8_t *data, size_t len) : data_(data), len_(len) {}

  uint8_t *get_data() const { return data_; }
  size_t get_len() const { return len_; }

private:
  uint8_t *data_;
  size_t len_;
};

class DS28E17I2CBusWriteBuffer {
public:
  DS28E17I2CBusWriteBuffer(const uint8_t *data, size_t len) : data_(data), len_(len) {}

  const uint8_t *get_data() const { return data_; }
  size_t get_len() const { return len_; }

private:
  const uint8_t *data_;
  size_t len_;
};

class DS28E17I2CBusComponent : public esphome::i2c::I2CComponent, public esphome::Component {
 public:
  DS28E17I2CBusComponent(ESPOneWire *one_wire, uint64_t address);

  void setup() override;
  float get_setup_priority() const override;

  void set_one_wire(ESPOneWire *one_wire);

  void wakeUp();
  void enableSleepMode();

  bool write(uint8_t i2cAddress, uint8_t *data, uint8_t dataLength);
  bool memoryWrite(uint8_t i2cAddress, uint8_t i2cRegister, uint8_t *data, uint8_t dataLength);
  bool read(uint8_t i2cAddress, uint8_t *buffer, uint8_t bufferLength);
  bool memoryRead(uint8_t i2cAddress, uint16_t i2cRegister, uint8_t *buffer, uint8_t bufferLength);
  void set_i2c_speed(const std::string &i2c_speed) { i2c_speed_ = i2c_speed; }
  bool write_configuration(uint8_t config_byte);

 protected:
  ESPOneWire *one_wire_;
  uint64_t address_;

  bool write_bytes(uint8_t address, const uint8_t *data, uint8_t len) override;
  bool read_bytes(uint8_t address, uint8_t *data, uint8_t len) override;

  esphome::ErrorCode readMultiple(uint8_t i2cAddress, DS28E17I2CBusReadBuffer *buffers, size_t cnt);
  esphome::ErrorCode writeMultiple(uint8_t i2cAddress, DS28E17I2CBusWriteBuffer *buffers, size_t cnt);
  esphome::ErrorCode writeReadMultiple(uint8_t i2cAddress, DS28E17I2CBusWriteBuffer *writeBuffers, size_t writeCnt, DS28E17I2CBusReadBuffer *readBuffers, size_t readCnt);

  void configure_i2c_speed() {
    uint8_t config_byte;
    if (i2c_speed_ == "standard") {
      config_byte = 0x02;
    } else if (i2c_speed_ == "fast") {
      config_byte = 0x04;
    } else {
      ESP_LOGE(TAG, "Invalid i2c_speed value. Allowed values: 'standard', 'fast'");
      return;
    }
    write_configuration(config_byte); // Call write_configuration with the config_byte
  }

  std::string i2c_speed_;

 private:
  bool _writeTo(uint8_t *header, uint8_t headerLength, uint8_t *data, uint8_t dataLength);
  bool _readFrom(uint8_t *header, uint8_t headerLength, uint8_t *buffer, uint8_t bufferLength);
  enum UpdateState {
    IDLE,
    BEGIN_TRANSMISSION,
    UPDATE_DEVICE,
    END_TRANSMISSION,
  };

  UpdateState update_state_;
};

} // namespace ds28e17_i2c_bus