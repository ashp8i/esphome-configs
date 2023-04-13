#pragma once

#include "esphome/core/hal.h"
#include <vector>

namespace esphome {
namespace onewire {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const int ONE_WIRE_ROM_SEARCH;

class CustomOneWire {
 public:
  CustomOneWire(InternalGPIOPin *pin, InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, bool low_power_mode = false, bool overdrive_mode = false);

  /** Reset the bus, should be done before all write operations.
   *
   * Takes approximately 1ms.
   *
   * @return Whether the operation was successful.
   */
  bool reset();

  /// Write a single bit to the bus, takes about 70µs.
  void write_bit(bool bit);

  /// Read a single bit from the bus, takes about 70µs
  bool read_bit();

  /// Write a word to the bus. LSB first.
  void write8(uint8_t val);

  /// Write a 64 bit unsigned integer to the bus. LSB first.
  void write64(uint64_t val);

  /// Write a command to the bus that addresses all devices by skipping the ROM.
  void skip();

  /// Read an 8 bit word from the bus.
  uint8_t read8();

  /// Read an 64-bit unsigned integer from the bus.
  uint64_t read64();

  /// Select a specific address on the bus for the following command.
  void select(uint64_t address);

  /// Reset the device search.
  void reset_search();

  /// Search for a 1-Wire device on the bus. Returns 0 if all devices have been found.
  uint64_t search();

  /// Helper that wraps search in a std::vector.
  std::vector<uint64_t> search_vec();

  // New method to enable/disable resumeDevice functionality
  /** Enables or disables the resumeDevice functionality for a specific device with the given ROM number.
   *  The strongPullupPin parameter is used to specify the GPIO pin to use for the strong pull-up during resumeDevice.
   *  The enableResumeDevice parameter is used to specify whether to enable or disable the resumeDevice functionality.
   */
  void resumeDevice(uint8_t* rom, int strongPullupPin, bool enableResumeDevice);

  // Add methods that the ds28e17_i2c_bus_component might need
  /** Writes data to a DS28E17 device at the specified address on the bus.
   *  The data parameter is a pointer to the data to write, and the len parameter specifies the length of the data in bytes.
   *  Returns true if the write operation was successful, false otherwise.
   */
  bool ds28e17_write_data(uint64_t address, uint8_t *data, uint8_t len);

  /** Reads data from a DS28E17 device at the specified address on the bus.
   *  The data parameter is a pointer to the buffer to store the read data, and the len parameter specifies the length of the buffer in bytes.
   *  Returns true if the read operation was successful, false otherwise.
   */
  bool ds28e17_read_data(uint64_t address, uint8_t *data, uint8_t len);

  /** Writes a command to a DS28E17 device at the specified address on the bus.
   *  The command parameter is the command to write.
   *  Returns true if the write operation was successful, false otherwise.
   */
  bool ds28e17_write_command(uint64_t address, uint8_t command);

  /// Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.
  uint8_t* get_rom_number8() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

  // Parasitic Power Support - depower
  void depower();


 private:
  InternalGPIOPin *pin_;
  CustomOneWire one_wire_;
  enum class State {
    Reset,
    PresenceDetection,
    SelectRom,
    Read,
    Write,
    Sleep,
  };
  bool low_power_mode_; // new member variable
  void sleep_() {
    if (low_power_mode_) { // only issue sleep command if low-power mode is enabled
      reset_();
      write_(0xCC); // Skip Rom command
      write_(0xB4); // Issue a sleep command
      state_ = State::Sleep; // transition to sleep state
    }
  }

  void resume_() {
    if (low_power_mode_) { // only issue resume command if low-power mode is enabled
      reset_();
      write_(0xCC); // Skip Rom command
      write_(0xA5); // Issue a resume command
      state_ = State::Reset; // transition back to reset state
    }
  }
  bool overdrive_mode_; // new member variable for overdrive mode
    void start_overdrive_() {
    if (overdrive_mode_) {
      reset_();
      write_(0x3C); // Overdrive Skip Rom command
      state_ = State::PresenceDetection;
    }
    }

  bool crc_check_(const uint8_t *data, int len, uint8_t crc) {
    uint8_t computed_crc = 0;
    for (int i = 0; i < len; i++) {
      uint8_t byte = data[i];
      for (int j = 0; j < 8; j++) {
        uint8_t bit = (byte ^ computed_crc) & 0x01;
        computed_crc >>= 1;
        byte >>= 1;
        if (bit) {
          computed_crc ^= 0x8C;
        }
      }
    }
    return computed_crc == crc;
  }
  bool parasitic_power_mode_;
  void pulse_pin(bool value);

 protected:
  /// Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.
  inline uint8_t *rom_number8_();
  // ISRInternalGPIOPin pin_;
  uint8_t last_discrepancy_{0};
  bool last_device_flag_{false};
  uint64_t rom_number_{0};
    
};

class OneWireModeTracker : public Component, public Sensor {
 public:
  void setup() override;
  void update() override;

 private:
  CustomOneWire* custom_one_wire_;
  int current_mode_ = normal_mode;
  int last_mode_ = normal_mode;
};

void OneWireModeTracker::setup() {
  // Create the custom OneWire component
  custom_one_wire_ = new CustomOneWire(&InternalGPIOPin(GPIO_NUM_4), false, false);
  custom_one_wire_->set_client_id("my_custom_one_wire");
  App.register_component(custom_one_wire_);

  // Expose the current mode as a sensor
  set_name("OneWire Mode");
  set_icon("mdi:flash");
  add_on_state_callback([this](std::string state) {
    this->publish_state(state);
  });

  // Initialize the state machine with the current mode
  if (custom_one_wire_->get_mode_id() == normal_mode) {
    current_mode_ = normal_mode;
    publish_state("Normal Mode");
  } else if (custom_one_wire_->get_mode_id() == low_power_mode) {
    current_mode_ = low_power_mode;
    publish_state("Low Power Mode");
  } else if (custom_one_wire_->get_mode_id() == overdrive_mode) {
    current_mode_ = overdrive_mode;
    publish_state("Overdrive Mode");
  } else {
    current_mode_ = normal_mode;
    publish_state("Unknown Mode");
  }
}

void OneWireModeTracker::update() {
  // Check the current mode and update the sensor state if necessary
  std::string current_mode_string;
  if (custom_one_wire_->get_mode_id() == normal_mode) {
    current_mode_string = "Normal Mode";
    current_mode_ = normal_mode;
  } else if (custom_one_wire_->get_mode_id() == low_power_mode) {
    current_mode_string = "Low Power Mode";
    current_mode_ = low_power_mode;
  } else if (custom_one_wire_->get_mode_id() == overdrive_mode) {
    current_mode_string = "Overdrive Mode";
    current_mode_ = overdrive_mode;
  } else {
    current_mode_string = "Unknown Mode";
    current_mode_ = normal_mode;
  }

  if (current_mode_ != last_mode_) {
    last_mode_ = current_mode_;
    publish_state(current_mode_string);
  }
}

}  // namespace onewire
}  // namespace esphome