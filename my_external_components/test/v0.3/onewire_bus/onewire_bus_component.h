#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include <vector>

namespace esphome {
namespace onewire_bus {

extern const uint8_t ONE_WIRE_ROM_SELECT;
extern const int ONE_WIRE_ROM_SEARCH;

class OneWireBusComponent : public Component {
 public:
  //Pin Setup
  explicit OneWireBusComponent(InternalGPIOPin *pin, InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, bool low_power_mode = false, bool overdrive_mode = false);
  void set_pin(InternalGPIOPin *pin) { pin_ = pin; }                          // Set pin
  void register_device(OneWireBusComponent *device);                          // Register Device Callback
  void setup() override;                                                      // Component Setup
  void dump_config() override;                                                // Read Configuration
  float get_setup_priority() const override { return setup_priority::DATA; }  // Component Setup priority

  void update() override;  // Update Method
  std::vector<uint64_t> get_found_devices() { return found_devices_; } // Vector for returning found devices

  uint8_t *get_address8();                        // Helper to create (and cache) the name for this Device. For example "0xfe0000031f1eaf29".
  const std::string &get_address_name();          // Address Name Return Method
  void set_address(uint64_t address);             // Set the 64-bit unsigned address for this Device.
  optional<uint8_t> get_index() const;            // Get the index of this Device. (0 if using address.)
  void set_index(uint8_t index);                  // Set the index, if using index, address will be set after setup.
  // uint8_t get_resolution() const;                 // Get the set resolution for this sensor.
  // void set_resolution(uint8_t resolution);        // Set the resolution for this sensor.
  // uint16_t millis_to_wait_for_conversion() const; // Get the number of milliseconds we have to wait for the conversion phase.
  // bool setup_device();                            // Setup Device
  // bool read_scratch_pad();                        // Read Scratch Pad
  // bool check_scratch_pad();                       // Check Scratch Pad
  // float get_temp_c();                             // Get Temperature in Degrees C
  // std::string unique_id() override;               // Unique ID

  bool reset();                        // Reset, used before write operations performed, takes 1ms, @return validation
  void select(uint64_t address);       // Select a specific address for following command
  void skip();                         // Write command to all devices by skipping the ROM.
  void write_bit(bool bit);            // Write single bit 70µs.
  void write8(uint8_t val);            // Write a word, LSB first.
  void write64(uint64_t val);          // Write a 64 bit unsigned integer, LSB first.
  bool read_bit();                     // Read single bit 70µs.
  uint8_t read8();                     /// Read an 8 bit word.
  uint64_t read64();                   // Read an 64-bit unsigned integer
  
  // Search
  void reset_search();                 // Reset the device search.
  uint64_t search();                   // Search for devices, Returns 0 if all devices have been found.
  std::vector<uint8_t> device_id_ = std::vector<uint8_t>(8, 0x00);
  std::vector<uint64_t> search_vec();  // Helper that wraps search in a std::vector.
  // uint8_t crc_ = 0;
  // uint8_t last_family_discrepancy_{0};
  // uint8_t last_family_result_{0};
  // uint8_t last_device_address_[8] = {0};

  // functions for reading and writing data to the scratch pad
  // bool read_scratch_pad(uint8_t* rom);
  // bool write_scratch_pad(uint8_t* rom, uint8_t* scratchpad);
  // bool copy_scratch_pad(uint8_t* rom);

  // void match_rom(uint8_t* rom);
  // void skip_rom();
  // void search_rom();
  // void target_search(uint8_t family_code);
  // bool verify_crc();
  // bool validate_address(uint8_t* addr);
  // uint8_t crc8(uint8_t* data, uint8_t len);

  // void reset_search();
  // bool search(uint8_t* newAddr, bool search_mode);
  // void target_setup(uint8_t family_code, uint8_t* new_addr, bool search_mode);
  // bool read_bit();
  // void write_bit(bool bit_value);
  // void write_byte(uint8_t byte_value);
  // uint8_t read_byte();
  
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
  void depower();                      // Parasitic Power Support - depower

 private:
  InternalGPIOPin *pin_;
  OneWireBusComponent *one_wire_;                         // one wire pointer
  enum class State {
    Reset,
    PresenceDetection,
    SelectRom,
    Read,
    Write,
    Sleep,
  }
  State state_; // declare state_ member variable
  bool low_power_mode_; // new member variable
  void sleep_() {
    if (low_power_mode_) { // only issue sleep command if low-power mode is enabled
      reset();
      write8(0xCC); // Skip Rom command
      write8(0xB4); // Issue a sleep command
      state_ = State::Sleep; // transition to sleep state
    }
  }

  void resume_() {
    if (low_power_mode_) { // only issue resume command if low-power mode is enabled
      reset();
      write8(0xCC); // Skip Rom command
      write8(0xA5); // Issue a resume command
      state_ = State::Reset; // transition back to reset state
    }
  }
  bool overdrive_mode_; // new member variable for overdrive mode
    void start_overdrive_() {
    if (overdrive_mode_) {
      reset();
      write8(0x3C); // Overdrive Skip Rom command
      state_ = State::PresenceDetection;  // Detect Pulse
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
  // static const uint8_t kPresenceDetectWait = 60;
  // static const uint8_t kOverdrivePresenceDetectWait = 2;
  // static const uint8_t kReadSlotTime = 8;
  // static const uint8_t kWriteSlotTime = 8;
  // static const uint8_t kOverdriveReadSlotTime = 1;
  // static const uint8_t kOverdriveWriteSlotTime = 3;

  // static const uint8_t kCmdSearchROM = 0xF0;
  // static const uint8_t kCmdReadROM = 0x33;
  // static const uint8_t kCmdMatchROM = 0x55;
  // static const uint8_t kCmdSkipROM = 0xCC;
  // static const uint8_t kCmdAlarmSearch = 0xEC;
  // static const uint8_t kCmdResume = 0xA5;

  // static const uint8_t kFamilyCode_DS18B20 = 0x28;
  // static const uint8_t kFamilyCode_DS18S20 = 0x10;
  // static const uint8_t kFamilyCode_DS1822 = 0x22;
  // static const uint8_t kFamilyCode_DS18B20P = 0x42;
  // static const uint8_t kFamilyCode_DS28EA00 = 0x42;
  // static const uint8_t kFamilyCode_DS28E17 = 0x1D;
  // static const uint8_t kFamilyCode_DS2406 = 0x12;

  // static const uint8_t kScratchPadSize_DS18B20 = 9;
  // static const uint8_t kScratchPadSize_DS18S20 = 9;
  // static const uint8_t kScratchPadSize_DS1822 = 9;
  // static const uint8_t kScratchPadSize_DS18B20P = 17;
  // static const uint8_t kScratchPadSize_DS28EA00 = 17;
  // static const uint8_t kScratchPadSize_DS28E17 = 64;
  // static const uint8_t kScratchPadSize_DS2406 = 8;

  // static const uint8_t kScratchPadTemperature_LSB = 0;
  // static const uint8_t kScratchPadTemperature_MSB = 1;
  // static const uint8_t kScratchPadAlarmHigh = 2;
  // static const uint8_t kScratchPadAlarmLow = 3;
  // static const uint8_t kScratchPadConfiguration = 4;
  // static const uint8_t kScratchPadInternalByte = 5;
  // static const uint8_t kScratchPadCountRemain = 6;
  // static const uint8_t kScratchPadCountPerC = 7;
  // static const uint8_t kScratchPadCRC = 8;

  // static const uint8_t kAlarmSearchComparisonMask = 0x01;

  // static const uint8_t kPowerSupplyParasite = 0x00;
  // static const uint8_t kPowerSupplyExternal = 0x01;

 protected:
  //ISRInternalGPIOPin pin_;                                // ISR Pin Pointer
  //ISRInternalGPIOPin isr_pin_;                            // ISR Pin Pointer
  // InternalGPIOPin *pin_;                                  // Pin Pointer
  uint8_t last_discrepancy_{0}                            // Scratch Pad Error Checking
  bool last_device_flag_{false}                           // last device?
  uint64_t rom_number_{0}                                 // Unsigned 64-bit ROM Number
  inline uint8_t *rom_number8_();                         // Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.
  // OneWireBusComponent *one_wire_;                         // one wire pointer
  std::vector<OneWireBusComponent *> devices_;            // device prefix
  std::vector<uint64_t> found_devices_;                   // Vector for devices returned from a search
  uint64_t address_;                                      // Unsigned 64-bit ROM Address
  optional<uint8_t> index_;                               // Assign Index
  // uint8_t resolution_;                                    // Resolution
  std::string address_name_;                              // Device Name String
  uint8_t scratch_pad_[9] = {
      0,
  };  // Unsigned 8-bit Scratch Pad Store
};

}  // namespace onewire_bus
}  // namespace esphome
