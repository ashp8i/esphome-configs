
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
  // Pin Setup
  explicit OneWireBusComponent(InternalGPIOPin* pin, InternalGPIOPin* in_pin, InternalGPIOPin* out_pin, bool low_power_mode = false, bool overdrive_mode = false, bool parasitic_power_mode = false)
      : pin_(pin), in_pin_(in_pin), out_pin_(out_pin), low_power_mode_(low_power_mode), overdrive_mode_(overdrive_mode), parasitic_power_mode_(parasitic_power_mode), found_devices_() {}

  // virtual ~OneWireBusComponent();                                             // Deconstructor declaration virtual
  // function
  void set_pins(InternalGPIOPin *pin, InternalGPIOPin *in_pin, InternalGPIOPin *out_pin) {
  pin_ = pin;
  in_pin_ = in_pin;
  out_pin_ = out_pin;
}                          // Set pins
  // void register_device(OneWireBusComponent *device);                          // Register Device Callback
  void register_device(OneWireBusComponent *device);                          // Register Device Callback
  void setup() override;                                                      // Component Setup
  void dump_config() override;                                                // Read Configuration
  float get_setup_priority() const override { return setup_priority::DATA; }  // Component Setup priority

  void update();                                                        // Update Method - override removed
  std::vector<uint64_t> get_found_devices() { return found_devices_; }  // Vector for returning found devices

  uint8_t *get_address8();  // Helper to create (and cache) the name for this Device. For example "0xfe0000031f1eaf29".
  const std::string &get_address_name();  // Address Name Return Method
  void set_address(uint64_t address);     // Set the 64-bit unsigned address for this Device.
  optional<uint8_t> get_index() const;    // Get the index of this Device. (0 if using address.)
  void set_index(uint8_t index);          // Set the index, if using index, address will be set after setup.
  // uint8_t get_resolution() const;                 // Get the set resolution for this sensor.
  // void set_resolution(uint8_t resolution);        // Set the resolution for this sensor.
  // uint16_t millis_to_wait_for_conversion() const; // Get the number of milliseconds we have to wait for the
  // conversion phase. bool setup_device();                            // Setup Device bool read_scratch_pad(); // Read
  // Scratch Pad bool check_scratch_pad();                       // Check Scratch Pad float get_temp_c(); // Get
  // Temperature in Degrees C std::string unique_id() override;               // Unique ID

  bool reset();                   // Reset, used before write operations performed, takes 1ms, @return validation
  void select(uint64_t address);  // Select a specific address for following command
  void skip();                    // Write command to all devices by skipping the ROM.
  void write_bit(bool bit);       // Write single bit 70µs.
  void write8(uint8_t val);       // Write a word, LSB first.
  void write64(uint64_t val);     // Write a 64 bit unsigned integer, LSB first.
  bool read_bit();                // Read single bit 70µs.
  uint8_t read8();                /// Read an 8 bit word.
  uint64_t read64();              // Read an 64-bit unsigned integer

  // Search
  void reset_search();  // Reset the device search.
  uint64_t search();    // Search for devices, Returns 0 if all devices have been found.
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
  void resumeDevice(uint8_t *rom, int strongPullupPin, bool enableResumeDevice);

  // Add methods that the ds28e17_i2c_bus_component might need
  /** Writes data to a DS28E17 device at the specified address on the bus.
   *  The data parameter is a pointer to the data to write, and the len parameter specifies the length of the data in
   * bytes. Returns true if the write operation was successful, false otherwise.
   */
  bool ds28e17_write_data(uint64_t address, uint8_t *data, uint8_t len);

  /** Reads data from a DS28E17 device at the specified address on the bus.
   *  The data parameter is a pointer to the buffer to store the read data, and the len parameter specifies the length
   * of the buffer in bytes. Returns true if the read operation was successful, false otherwise.
   */
  bool ds28e17_read_data(uint64_t address, uint8_t *data, uint8_t len);

  /** Writes a command to a DS28E17 device at the specified address on the bus.
   *  The command parameter is the command to write.
   *  Returns true if the write operation was successful, false otherwise.
   */
  bool ds28e17_write_command(uint64_t address, uint8_t command);
  void depower();  // Parasitic Power Support - depower

 private:
  InternalGPIOPin *pin_;                 // Pin Declaration
  InternalGPIOPin *in_pin_;              // In Pin Declaration
  InternalGPIOPin *out_pin_;             // Out Pin Declaration
  bool low_power_mode_;                  // New Member Variable for Low Power Mode
  bool overdrive_mode_;                  // New Member Variable for Overdrive Mode
  bool parasitic_power_mode_;            // New Member Variable for Parasitic Power Mode
  void pulse_pin(bool value);            // New Member Variable for Pulse Pin
  std::vector<uint64_t> found_devices_;  // Vector for devices returned from a search
  OneWireBusComponent *one_wire_;        // one wire pointer
  enum class State {
    Reset,
    PresenceDetection,
    SelectRom,
    Read,
    Write,
    Sleep,
  };
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

 protected:
  uint8_t last_discrepancy_{0};    // Scratch Pad Error Checking
  bool last_device_flag_{false};   // last device?
  uint64_t rom_number_{0};         // Unsigned 64-bit ROM Number
  inline uint8_t *rom_number8_();  // Helper to get the internal 64-bit unsigned rom number as a 8-bit integer pointer.
  std::vector<OneWireBusComponent *> devices_;  // device prefix
  uint64_t address_;                            // Unsigned 64-bit ROM Address
  optional<uint8_t> index_;                     // Assign Index
  // uint8_t resolution_;                                    // Resolution
  std::string address_name_;  // Device Name String
  uint8_t scratch_pad_[9] = {
      0,
  };  // Unsigned 8-bit Scratch Pad Store
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

}  // namespace onewire_bus
}  // namespace esphome
