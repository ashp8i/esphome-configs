// #ifdef USE_ARDUINO // May not need as the Library is Platform Agnostic

#include "onewire_bus_component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstdint>

namespace esphome {
namespace onewire_bus {

static const char *const TAG = "dallas_maxim.one_wire";

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const int ONE_WIRE_ROM_SEARCH = 0xF0;

/*
uint16_t OneWireBusComponent::millis_to_wait_for_conversion() const {
  switch (this->resolution_) {
    case 9:
      return 94;
    case 10:
      return 188;
    case 11:
      return 375;
    default:
      return 750;
  }
}
*/

/*bad constructor
OneWireBusComponent::OneWireBusComponent(InternalGPIOPin *pin) : pin_(pin), isr_pin_(pin) {}
*/

auto one_wire_ = new OneWireBusComponent{ .pin = pin, .in_pin = in_pin, .out_pin = out_pin };
{
    // Check if the pin supports PWM
    if (!pin_->is_pwm()) {
      ESP_LOGE(TAG, "Pin %d does not support PWM", pin_->get_pin());
      return;
    }

    // Check if the PWM channel is available
    int pwm_channel = pin_->get_pwm_channel();

#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32)
    if (ledc_get_pwm_channel_status(pwm_channel) != LEDC_CHANNEL_FREE) {
      ESP_LOGE(TAG, "PWM channel %d is already in use", pwm_channel);
      return;
    }
#elif defined(ESP8266)
    if (analogWrite(pin_->get_pin(), 0) == 0) {
      ESP_LOGE(TAG, "PWM channel %d is already in use", pwm_channel);
      return;
    }
#endif

    // All checks passed, enable PWM output on the pin
    pin_->setup_pwm();
  }

  if (in_pin && out_pin) {
    // Split I/O mode
    in_pin_ = in_pin->to_isr();
    out_pin_ = out_pin->to_isr();
  } else {
    // Single-pin mode
    pin_ = pin_->to_isr();
  }
}

// ~OneWireBusComponent() {
//   delete pin_;
//   delete in_pin_;
//   delete out_pin_;
// }

/*
bool HOT IRAM_ATTR OneWireBusComponent::reset() {
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  // Wait for communication to clear (delay G)
  this->pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  uint8_t retries = 125;
  do {
    if (--retries == 0)
      return false;
    delayMicroseconds(2);
  } while (!this->pin_.digital_read());

  // Send 480µs LOW TX reset pulse (drive bus low, delay H)
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  this->pin_.digital_write(false);
  delayMicroseconds(480);

  // Release the bus, delay I
  this->pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(70);

  // sample bus, 0=device(s) present, 1=no device present
  bool r = !this->pin_.digital_read();
  // delay J
  delayMicroseconds(410);
  return r;
}
*/

bool HOT IRAM_ATTR OneWireBusComponent::reset() {
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  // Wait for communication to clear (delay G)
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  uint8_t retries = 125;
  do {
    if (--retries == 0)
      return false;
    delayMicroseconds(2);
  } while (!pin_.digital_read());

  // Send 480µs LOW TX reset pulse (drive bus low, delay H)
  pin_->pin_mode(gpio::FLAG_OUTPUT);
  pin_->digital_write(false);
  delayMicroseconds(480);

  // Release the bus, delay I
  pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(70);

  // sample bus, 0=device(s) present, 1=no device present
  bool presence = !pin_->digital_read();
  // delay J
  delayMicroseconds(410);
  return presence;
}

/*
void HOT IRAM_ATTR OneWireBusComponent::write_bit(bool bit) {
  // drive bus low
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  this->pin_.digital_write(false);

  // from datasheet:
  // write 0 low time: t_low0: min=60µs, max=120µs
  // write 1 low time: t_low1: min=1µs, max=15µs
  // time slot: t_slot: min=60µs, max=120µs
  // recovery time: t_rec: min=1µs
  // ds18b20 appears to read the bus after roughly 14µs
  uint32_t delay0 = bit ? 6 : 60;
  uint32_t delay1 = bit ? 54 : 5;

  // delay A/C
  delayMicroseconds(delay0);
  // release bus
  this->pin_.digital_write(true);
  // delay B/D
  delayMicroseconds(delay1);
}
*/

void HOT IRAM_ATTR OneWireBusComponent::write_bit(bool bit) {
  // drive bus low
  pin_->pin_mode(gpio::FLAG_OUTPUT);
  pin_->digital_write(false);

  // from datasheet:
  // write 0 low time: t_low0: min=60µs, max=120µs
  // write 1 low time: t_low1: min=1µs, max=15µs
  // time slot: t_slot: min=60µs, max=120µs
  // recovery time: t_rec: min=1µs
  // ds18b20 appears to read the bus after roughly 14µs
  uint32_t const delay0 = bit ? 6 : 60;
  uint32_t const delay1 = bit ? 54 : 5;

  // delay A/C
  delayMicroseconds(delay0);
  // release bus
  pin_->digital_write(true);
  // delay B/D
  delayMicroseconds(delay1);
}

/*
bool HOT IRAM_ATTR OneWireBusComponent::read_bit() {
  // drive bus low
  this->pin_.pin_mode(gpio::FLAG_OUTPUT);
  this->pin_.digital_write(false);

  // note: for reading we'll need very accurate timing, as the
  // timing for the digital_read() is tight; according to the datasheet,
  // we should read at the end of 16µs starting from the bus low
  // typically, the ds18b20 pulls the line high after 11µs for a logical 1
  // and 29µs for a logical 0

  uint32_t start = micros();
  // datasheet says >1µs
  delayMicroseconds(3);

  // release bus, delay E
  this->pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  // Unfortunately some frameworks have different characteristics than others
  // esp32 arduino appears to pull the bus low only after the digital_write(false),
  // whereas on esp-idf it already happens during the pin_mode(OUTPUT)
  // manually correct for this with these constants.

#ifdef USE_ESP32
  uint32_t timing_constant = 12;
#else
  uint32_t timing_constant = 14;
#endif

  // measure from start value directly, to get best accurate timing no matter
  // how long pin_mode/delayMicroseconds took
  while (micros() - start < timing_constant)
    ;

  // sample bus to read bit from peer
  bool r = this->pin_.digital_read();

  // read slot is at least 60µs; get as close to 60µs to spend less time with interrupts locked
  uint32_t now = micros();
  if (now - start < 60)
    delayMicroseconds(60 - (now - start));

  return r;
}

bool HOT IRAM_ATTR OneWireBusComponent::read_bit() {
  isr_pin_.digital_write(false);
  delayMicroseconds(10);
  isr_pin_.digital_write(true);
  delayMicroseconds(10);
  bool bit = isr_pin_.digital_read();
  delayMicroseconds(50);
  return bit;
}
*/

bool HOT IRAM_ATTR OneWireBusComponent::read_bit() {
  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);

  // note: for reading we'll need very accurate timing, as the
  // timing for the digital_read() is tight; according to the datasheet,
  // we should read at the end of 16µs starting from the bus low
  // typically, the ds18b20 pulls the line high after 11µs for a logical 1
  // and 29µs for a logical 0

  uint32_t const start = micros();
  // datasheet says >1µs
  delayMicroseconds(3);

  // release bus, delay E
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  // Unfortunately some frameworks have different characteristics than others
  // esp32 arduino appears to pull the bus low only after the digital_write(false),
  // whereas on esp-idf it already happens during the pin_mode(OUTPUT)
  // manually correct for this with these constants.

#ifdef USE_ESP32
  uint32_t timing_constant = 12;
#else
  uint32_t const timing_constant = 14;
#endif

  // measure from start value directly, to get best accurate timing no matter
  // how long pin_mode/delayMicroseconds took
  while (micros() - start < timing_constant)
    ;

  // sample bus to read bit from peer
  bool bit = pin_.digital_read();

  // read slot is at least 60µs; get as close to 60µs to spend less time with interrupts locked
  uint32_t const now = micros();
  if (now - start < 60)
    delayMicroseconds(60 - (now - start));

  return bit;
}

/*
void IRAM_ATTR OneWireBusComponent::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit(bool((1u << i) & val));
  }
}
*/

void IRAM_ATTR OneWireBusComponent::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit(bool((1u << i) & val));
  }
}

/*
void IRAM_ATTR OneWireBusComponent::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit(bool((1ULL << i) & val));
  }
}
*/

void IRAM_ATTR OneWireBusComponent::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit(bool((1ULL << i) & val));
  }
}

/*
uint8_t IRAM_ATTR OneWireBusComponent::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit()) << i);
  }
  return ret;
}
*/

uint8_t IRAM_ATTR OneWireBusComponent::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit()) << i);
  }
  return ret;
}

/*
uint64_t IRAM_ATTR OneWireBusComponent::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit()) << i);
  }
  return ret;
}
*/
uint64_t IRAM_ATTR OneWireBusComponent::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit()) << i);
  }
  return ret;
}

/*
void IRAM_ATTR OneWireBusComponent::select(uint64_t address) {
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
}
*/

void IRAM_ATTR OneWireBusComponent::select(uint64_t address) {
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
}

/*
void IRAM_ATTR OneWireBusComponent::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->rom_number_ = 0;
}
*/

void IRAM_ATTR OneWireBusComponent::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->rom_number_ = 0;
}

/*
uint64_t IRAM_ATTR OneWireBusComponent::search() {
  if (this->last_device_flag_) {
    return 0u;
  }

  {
    InterruptLock lock;
    if (!this->reset()) {
      // Reset failed or no devices present
      this->reset_search();
      return 0u;
    }
  }

  uint8_t id_bit_number = 1;
  uint8_t last_zero = 0;
  uint8_t rom_byte_number = 0;
  bool search_result = false;
  uint8_t rom_byte_mask = 1;

  {
    InterruptLock lock;
    // Initiate search
    this->write8(ONE_WIRE_ROM_SEARCH);
    do {
      // read bit
      bool id_bit = this->read_bit();
      // read its complement
      bool cmp_id_bit = this->read_bit();

      if (id_bit && cmp_id_bit) {
        // No devices participating in search
        break;
      }

      bool branch;

      if (id_bit != cmp_id_bit) {
        // only chose one branch, the other one doesn't have any devices.
        branch = id_bit;
      } else {
        // there are devices with both 0s and 1s at this bit
        if (id_bit_number < this->last_discrepancy_) {
          branch = (this->rom_number8_()[rom_byte_number] & rom_byte_mask) > 0;
        } else {
          branch = id_bit_number == this->last_discrepancy_;
        }

        if (!branch) {
          last_zero = id_bit_number;
        }
      }

      if (branch) {
        // set bit
        this->rom_number8_()[rom_byte_number] |= rom_byte_mask;
      } else {
        // clear bit
        this->rom_number8_()[rom_byte_number] &= ~rom_byte_mask;
      }

      // choose/announce branch
      this->write_bit(branch);
      id_bit_number++;
      rom_byte_mask <<= 1;
      if (rom_byte_mask == 0u) {
        // go to next byte
        rom_byte_number++;
        rom_byte_mask = 1;
      }
    } while (rom_byte_number < 8);  // loop through all bytes
  }

  if (id_bit_number >= 65) {
    this->last_discrepancy_ = last_zero;
    if (this->last_discrepancy_ == 0) {
      // we're at root and have no choices left, so this was the last one.
      this->last_device_flag_ = true;
    }
    search_result = true;
  }

  search_result = search_result && (this->rom_number8_()[0] != 0);
  if (!search_result) {
    this->reset_search();
    return 0u;
  }

  return this->rom_number_;
}
*/

uint64_t IRAM_ATTR OneWireBusComponent::search() {
  if (this->last_device_flag_) {
    return 0u;
  }

  {
    InterruptLock const lock;
    if (!this->reset()) {
      // Reset failed or no devices present
      this->reset_search();
      return 0u;
    }
  }

  uint8_t id_bit_number = 1;
  uint8_t last_zero = 0;
  uint8_t rom_byte_number = 0;
  bool search_result = false;
  uint8_t rom_byte_mask = 1;

  {
    InterruptLock const lock;
    // Initiate search
    this->write8(ONE_WIRE_ROM_SEARCH);
    do {
      // read bit
      bool const id_bit = this->read_bit();
      // read its complement
      bool const cmp_id_bit = this->read_bit();

      if (id_bit && cmp_id_bit) {
        // No devices participating in search
        break;
      }

      bool branch;

      if (id_bit != cmp_id_bit) {
        // only chose one branch, the other one doesn't have any devices.
        branch = id_bit;
      } else {
        // there are devices with both 0s and 1s at this bit
        if (id_bit_number < this->last_discrepancy_) {
          branch = (this->rom_number8_()[rom_byte_number] & rom_byte_mask) > 0;
        } else {
          branch = id_bit_number == this->last_discrepancy_;
        }

        if (!branch) {
          last_zero = id_bit_number;
        }
      }

      if (branch) {
        // set bit
        this->rom_number8_()[rom_byte_number] |= rom_byte_mask;
      } else {
        // clear bit
        this->rom_number8_()[rom_byte_number] &= ~rom_byte_mask;
      }

      // choose/announce branch
      this->write_bit(branch);
      id_bit_number++;
      rom_byte_mask <<= 1;
      if (rom_byte_mask == 0u) {
        // go to next byte
        rom_byte_number++;
        rom_byte_mask = 1;
      }
    } while (rom_byte_number < 8);  // loop through all bytes
  }

  if (id_bit_number >= 65) {
    this->last_discrepancy_ = last_zero;
    if (this->last_discrepancy_ == 0) {
      // we're at root and have no choices left, so this was the last one.
      this->last_device_flag_ = true;
    }
    search_result = true;
  }

  search_result = search_result && (this->rom_number8_()[0] != 0);
  if (!search_result) {
    this->reset_search();
    return 0u;
  }

  return this->rom_number_;
}

/*
std::vector<uint64_t> OneWireBusComponent::search_vec() {
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}
*/

std::vector<uint64_t> OneWireBusComponent::search_vec() {
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}

/*
void IRAM_ATTR OneWireBusComponent::skip() {
  this->write8(0xCC);  // skip ROM
}
*/

void IRAM_ATTR OneWireBusComponent::skip() {
  this->write8(0xCC);  // skip ROM
}

// uint8_t IRAM_ATTR *OneWireBusComponent::rom_number8_() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

uint8_t IRAM_ATTR *OneWireBusComponent::rom_number8_() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

void OneWireBusComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up OneWireBusComponent...");

  // ... code related to pin setup and initialization ...
  pin_.setup();

  // clear bus with 480µs high, otherwise initial reset in search_vec() fails
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(480);

  // Create an instance of the OneWireBusComponent class
  one_wire_ = new OneWireBusComponent(pin_);  // NOLINT(cppcoreguidelines-owning-memory)

  std::vector<uint64_t> raw_devices;
  raw_devices = this->one_wire_->search_vec();

  // ... code for handling the raw_devices data ...
  for (auto &address : raw_devices) {
    auto *address8 = reinterpret_cast<uint8_t *>(&address);
    if (crc8(address8, 7) != address8[7]) {
      ESP_LOGW(TAG, "ONEWIRE device 0x%s has invalid CRC.", format_hex(address).c_str());
      continue;
    }
    // No specific device type checks
    ESP_LOGI(TAG, "Found device with address: 0x%s", format_hex(address).c_str());
    this->found_devices_.push_back(address);
  }

  for (auto *device : this->devices_) {
    if (device->get_index().has_value()) {
      if (*device->get_index() >= this->found_devices_.size()) {
        this->status_set_error();
        continue;
      }
      device->set_address(this->found_devices_[*device->get_index()]);
    }

    if (!device->setup_device()) {
      this->status_set_error();
    }
  }
}

/*broken config routine
void OneWireBusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "OneWireBusComponent:");
  LOG_PIN("  Pin: ", this->pin_);
  // LOG_UPDATE_INTERVAL(this);

  if (this->found_devices_.empty()) {
    ESP_LOGW(TAG, "  Found no devices!");
  } else {
    ESP_LOGD(TAG, "  Found devices:");
    for (auto &address : this->found_devices_) {
      ESP_LOGD(TAG, "    0x%s", format_hex(address).c_str());
    }
  }

  for (auto *device : this->devices_) {
    LOG_device("  ", "Device", device);
    if (device->get_index().has_value()) {
      ESP_LOGCONFIG(TAG, "    Index %u", *device->get_index());
      if (*device->get_index() >= this->found_devices_.size()) {
        ESP_LOGE(TAG, "Couldn't find device by index - not connected. Proceeding without it.");
        continue;
      }
    }
    ESP_LOGCONFIG(TAG, "    Address: %s", device->get_address_name().c_str());
    // No resolution check
  }
}
*/

void IRAM_ATTR OneWireBusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ESPOneWireComponent:");
  LOG_PIN("  Pin: ", pin_);
}

void OneWireBusComponent::register_device(OneWireBusComponent *device) { this->devices_.push_back(device); }

/*update method needs finalising again
void OneWireBusComponent::update() {
  this->status_clear_warning();

  bool result;
  {
    InterruptLock lock;
    result = this->one_wire_->reset();
  }
  if (!result) {
    ESP_LOGE(TAG, "Requesting conversion failed");
    this->status_set_warning();
    return;
  }

  {
    InterruptLock lock;
    this->one_wire_->skip();
    this->one_wire_->write8(ONEWIRE_COMMAND_START_CONVERSION);
  }

  for (auto *device : this->devices_) {
    this->set_timeout(device->get_address_name(), device->millis_to_wait_for_conversion(), [this, device] {
      bool res = device->read_scratch_pad();

      if (!res) {
        ESP_LOGW(TAG, "'%s' - Resetting bus for read failed!", device->get_name().c_str());
        this->status_set_warning();
        return;
      }
      if (!device->check_scratch_pad()) {
        this->status_set_warning();
        return;
      }

      device->process_scratch_pad();
    });
  }
}
*/

void OneWireBusComponent::set_address(uint64_t address) { this->address_ = address; }

// optional<uint8_t> OneWireBusComponent::get_index() const { return this->index_; }

void OneWireBusComponent::set_index(uint8_t index) { this->index_ = index; }

uint8_t *OneWireBusComponent::get_address8() { return reinterpret_cast<uint8_t *>(&this->address_); }

const std::string &OneWireBusComponent::get_address_name() {
  if (this->address_name_.empty()) {
    this->address_name_ = std::string("0x") + format_hex(this->address_);
  }

  return this->address_name_;
}

std::string OneWireBusComponent::unique_id() { return "OneWire-" + str_lower_case(format_hex(this->address_)); }

// void OneWireBusComponent::process_scratch_pad(const std::vector<uint8_t>& scratch_pad) {
//   // Parse the temperature value from the scratch pad and publish it.
//   float tempc = this->parse_temperature(scratch_pad);
//   ESP_LOGD(TAG, "'%s': Got Temperature=%.1f°C", this->get_name().c_str(), tempc);
//   this->publish_state(tempc);
// }

// float OneWireBusComponent::parse_temperature(const std::vector<uint8_t>& scratch_pad) const {
//   // Implement the temperature parsing logic here, based on the scratch pad data format for this device.
//   // Return the temperature value as a float.

//   // For example, if the device uses a scratch pad format with a 16-bit temperature value in
//   // the first two bytes of the scratch pad, where the temperature is represented in units of 0.0625°C,
//   // you can parse the temperature value as follows:

//   uint16_t temp_raw = (scratch_pad[1] << 8) | scratch_pad[0];
//   float tempc = static_cast<float>(temp_raw) * 0.0625f;
//   return tempc;
// }

}  // namespace onewire_bus
}  // namespace esphome
