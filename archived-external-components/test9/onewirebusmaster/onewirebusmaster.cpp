#include "onewirebus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "../../core/gpio.h"

#include <cstdint>

namespace esphome {
namespace onewirebus {

static const char *const TAG = "onewirebuscomponent.bus";

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const int ONE_WIRE_ROM_SEARCH = 0xF0;

OneWireBusComponent::OneWireBusComponent(InternalGPIOPin *pin) { pin_ = pin->to_isr(); }

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
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);
  delayMicroseconds(480);

  // Release the bus, delay I
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(70);

  // sample bus, 0=device(s) present, 1=no device present
  bool r = !pin_.digital_read();
  // delay J
  delayMicroseconds(410);
  return r;
}

void HOT IRAM_ATTR OneWireBusComponent::write_bit(bool bit) {
  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);

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
  pin_.digital_write(true);
  // delay B/D
  delayMicroseconds(delay1);
}

bool HOT IRAM_ATTR OneWireBusComponent::read_bit() {
  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);

  // note: for reading we'll need very accurate timing, as the
  // timing for the digital_read() is tight; according to the datasheet,
  // we should read at the end of 16µs starting from the bus low
  // typically, the ds18b20 pulls the line high after 11µs for a logical 1
  // and 29µs for a logical 0

  uint32_t start = micros();
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
  uint32_t timing_constant = 14;
#endif

  // measure from start value directly, to get best accurate timing no matter
  // how long pin_mode/delayMicroseconds took
  while (micros() - start < timing_constant)
    ;

  // sample bus to read bit from peer
  bool r = pin_.digital_read();

  // read slot is at least 60µs; get as close to 60µs to spend less time with interrupts locked
  uint32_t now = micros();
  if (now - start < 60)
    delayMicroseconds(60 - (now - start));

  return r;
}

void IRAM_ATTR OneWireBusComponent::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit(bool((1u << i) & val));
  }
}

void IRAM_ATTR OneWireBusComponent::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit(bool((1ULL << i) & val));
  }
}

uint8_t IRAM_ATTR OneWireBusComponent::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit()) << i);
  }
  return ret;
}
uint64_t IRAM_ATTR OneWireBusComponent::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit()) << i);
  }
  return ret;
}
void IRAM_ATTR OneWireBusComponent::select(uint64_t address) {
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
}
void IRAM_ATTR OneWireBusComponent::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->rom_number_ = 0;
}
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
std::vector<uint64_t> OneWireBusComponent::search_vec() {
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}

void IRAM_ATTR OneWireBusComponent::skip() {
  this->write8(0xCC);  // skip ROM
}

uint8_t IRAM_ATTR *OneWireBusComponent::rom_number8_() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

void OneWireBusComponent::add_device(std::unique_ptr<OneWireBusDevice> device) {
  this->devices_.push_back(std::move(device));
}

void OneWireBusComponent::add_device(OneWireBusDevice& device) {
  this->devices_.push_back(&device);
}

const uint8_t* OneWireBusComponent::get_address8() const {
  return this->address8_; 
}

const OneWireBusComponent::InternalGPIOPin* get_pin() const {
  return this->pin_; 
}

void OneWireBusComponent::set_address(uint64_t address) { this->address_ = address; }
uint8_t OneWireBusComponent::get_index() const { return this->index_; }
void OneWireBusComponent::set_index(uint8_t index) { this->index_ = index; }
uint8_t *OneWireBusComponent::get_address8() { return reinterpret_cast<uint8_t *>(&this->address_); }
std::string OneWireBusComponent::get_address_name(uint64_t address) {
  std::string address_name = "0x" + format_hex(address);
  return address_name; 
}

void OneWireBusComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up OneWireBusComponent...");
  this->pin_->setup();   
  // Clear bus 
  this->pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);  
  delayMicroseconds(480);
  // Create bus instance
  this->bus_ = new OneWireBusComponent(this->pin_);
  // Discover devices
  this->addresses_ = this->bus_->search();  
  // Check device types and register specifically
  for (auto &address : this->addresses_) {
    auto *address8 = reinterpret_cast<uint8_t *>(&address);
    if (crc8(address8, 7) != address8[7]) { 
      continue; 
    }
    switch (address8[0]) {
      case DALLAS_MODEL_DS18S20:
      case DALLAS_MODEL_DS1822: 
      {
        auto *sensor = new OneWireBusComponent(this, address);
        sensor->setup();
        break;
      }
      default:
        devices_.push_back(new OneWireBusDevice(this)); 
    }
  }
}

void OneWireBusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "OneWireBusComponent:");
  LOG_PIN("  Pin: ", this->pin_);
  if (this->addresses_.empty()) {
    ESP_LOGW(TAG, "  Found no devices!"); 
  } else {
    ESP_LOGD(TAG, "  Found %d devices:", this->addresses_.size());
    for (size_t i = 0; i < this->addresses_.size(); i++) {
      ESP_LOGD(TAG, "    0x%s", format_hex(this->addresses_[i]).c_str());
    }
  }
}

void OneWireBusComponent::loop() {
  for (auto *device : this->devices_) {
    device->loop(); 
  }
}

OneWireBusDevice::OneWireBusDevice(OneWireBusComponent *parent) 
    : parent_(parent) {}

void OneWireBusDevice::dump_config() {
  ESP_LOGCONFIG(TAG, "OneWire device: 0x%016llX", this->address_);
  // Child classes can override dump_config() to dump device-specific info
}

void OneWireBusDevice::loop() {
  // Child classes can override loop() to do any regular device tasks  
  // For example:
  // - Starting A/D conversions (DS2450)
  // - Setting timeouts to read values (DS18B20) 
  // - Checking digital input states (DS2408)
}

}  // namespace onewirebus
}  // namespace esphome
