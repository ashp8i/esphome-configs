#include "esp_one_wire.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace dallas {

static const char *const TAG = "dallas.one_wire";

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const int ONE_WIRE_ROM_SEARCH = 0xF0;

ESPOneWire::ESPOneWire(InternalGPIOPin pin_or_pins, uint8_t reset_pin = 0) 
    : pin_{pin_or_pins, pin_or_pins}, using_split_io_(reset_pin != 0) {
  if (using_split_io_) {
    auto pins = {pin_or_pins, pin_or_pins};
    this->pin_.input_pin_->pin_ = pins[0];
    this->pin_.output_pin_->pin_ = pins[1];
    this->pin_.pin_ = pins[reset_pin - 1]; 
  } else {
    this->pin_.pin_ = pin_or_pins; 
  }
}

bool HOT IRAM_ATTR ESPOneWire::reset() {
  if (!using_split_io_) { 
    // Handle the case where a single pin_ is used.
    // See reset here:
    // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
    // Wait for communication to clear (delay G)
    this->pin_.pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    uint8_t retries = 125;
    do {
      if (--retries == 0)
        return false; 
      delayMicroseconds(2);
    } while (!this->pin_.pin_->digital_read());

    // Send 480μs LOW TX reset pulse (drive bus low, delay H)
    this->pin_.pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->pin_.pin_->digital_write(false);
    delayMicroseconds(480);  

    // Release the bus, delay I
    this->pin_.pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    delayMicroseconds(70);  

    // sample bus, 0=device(s) present, 1=no device present
    bool r = !this->pin_.pin_->digital_read();
    // delay J  
    delayMicroseconds(410);
    return r;
  } else {
    SplitIOReset(); 
  }
}

void HOT IRAM_ATTR ESPOneWire::SplitIOReset() {
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  // Set output_pin() as output and high initially
  this->pin_.output_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_.output_pin_->digital_write(true);

  // Wait for communication to clear (delay G)
  this->pin_.input_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);     
  uint8_t retries = 125;
  do {
    if (--retries == 0)
      return false;
    delayMicroseconds(2);
  } while (!this->pin_.input_pin_->digital_read());

  // Send 480μs LOW TX reset pulse (drive bus low, delay H)
  this->pin_.output_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_.output_pin_->digital_write(false);
  delayMicroseconds(480);  

  // Release output bus
  this->pin_.output_pin_->pin_mode(gpio::FLAG_INPUT);

  // Sample input pin, 0=device(s) present, 1=no device present
  bool r = !this->pin_.input_pin_->digital_read();

  // Delay J - 410us recovery 
  delayMicroseconds(410);
  return r;
}

void HOT IRAM_ATTR ESPOneWire::write_bit(bool bit) {
  if (!using_split_io_) { 
    // Handle the case where a single pin_ is used.
    // drive bus low
    this->pin_.pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->pin_.pin_->digital_write(false);

    // from datasheet:
    // write 0 low time: t_low0: min=60μs, max=120μs
    // write 1 low time: t_low1: min=1μs, max=15μs
    // time slot: t_slot: min=60μs, max=120μs
    // recovery time: t_rec: min=1μs
    // ds18b20 appears to read the bus after roughly 14μs
    uint32_t delay0 = bit ? 6 : 60;
    uint32_t delay1 = bit ? 54 : 5;

    // delay A/C
    delayMicroseconds(delay0); 
    // release bus
    this->pin_.pin_->digital_write(true);  
    // delay B/D
    delayMicroseconds(delay1);
  } else {
    SplitIOWriteBit(bit); 
  } 
} 
  
void HOT IRAM_ATTR ESPOneWire::SplitIOWriteBit(boot bit) {
  // Handle the case where pins[0] and pins[1] are used.
  // Drive output bus low
  this->pin_.output_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_.output_pin_->digital_write(false);

  // from datasheet:
  // write 0 low time: t_low0: min=60µs, max=120µs
  // write 1 low time: t_low1: min=1µs, max=15µs
  // time slot: t_slot: min=60µs, max=120µs
  // recovery time: t_rec: min=1µs
  // ds18b20 appears to read the bus after roughly 14µs
  uint32_t delay0 = bit ? 6 : 60; 
  uint32_t delay1 = bit ? 54 : 5;

  // Delay A/C
  delayMicroseconds(delay0);

  // Release output bus
  this->pin_.output_pin_->pin_mode(gpio::FLAG_INPUT);

  // Set the output pin state back to high
  this->pin_.output_pin_->digital_write(true);

  // Delay B/D
  delayMicroseconds(delay1);
}

bool HOT IRAM_ATTR ESPOneWire::read_bit() {
  if (!using_split_io_) { 
    // Handle the case where a single pin_ is used.
    // drive bus low
    this->pin_.pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->pin_.pin_->digital_write(false);

    // note: for reading we'll need very accurate timing, as the  
    // timing for the digital_read() is tight; according to the datasheet,
    // we should read at the end of 16μs starting from the bus low  
    // typically, the ds18b20 pulls the line high after 11μs for a logical 1
    // and 29μs for a logical 0

    uint32_t start = micros();
    // datasheet says >1μs
    delayMicroseconds(3);

    // release bus, delay E  
    this->pin_.pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP); 

    // Unfortunately some frameworks have different characteristics than others  
    // esp32 arduino appears to pull the bus low only after the digital_write(false),
    // whereas on esp-idf it already happens during the pin_mode(OUTPUT)
    // manually correct for this with these constants.  

  #ifdef USE_ESP32
    uint32_t timing_constant = esp_timer_get_time_constant();
  #else
    uint32_t timing_constant = 14; 
  #endif

    // measure from start value directly, to get best accurate timing no matter
    // how long pin_mode/delayMicroseconds took
    while (micros() - start < timing_constant)
      ;

    // sample bus to read bit from peer  
    bool r = this->pin_.pin_->digital_read();

    // read slot is at least 60μs; get as close to 60μs to spend less time with interrupts locked
    uint32_t now = micros();
    if (now - start < 60)
      delayMicroseconds(60 - (now - start));

    return r;
  } else {
    return SplitIOReadBit();
  }
}

bool HOT IRAM_ATTR ESPOneWire::SplitIOReadBit() {
  // Handle the case where pins[0] and pins[1] are used.
  // Drive input bus low 
  this->pin_.input_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_.input_pin_->digital_write(false);
  this->pin_.output_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  // note: for reading we'll need very accurate timing, as the
  // timing for the digital_read() is tight; according to the datasheet,
  // we should read at the end of 16μs starting from the bus low
  // typically, the ds18b20 pulls the line high after 11μs for a logical 1
  // and 29μs for a logical 0

  uint32_t start = micros();
  // Datasheet says >1us
  delayMicroseconds(3);

  // Release input bus, delay E
  this->pin_.input_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  // Unfortunately some frameworks have different characteristics than others
  // esp32 arduino appears to pull the bus low only after the digital_write(false),
  // whereas on esp-idf it already happens during the pin_mode(OUTPUT)
  // manually correct for this with these constants.

  #ifdef USE_ESP32
    uint32_t timing_constant = esp_timer_get_time_constant(); 
  #else
    uint32_t timing_constant = 14;  
  #endif

  // measure from start value directly, to get best accurate timing no matter
  // how long pin_mode/delayMicroseconds took  
  while (micros() - start < timing_constant)
      ;

  // Sample input pin to read bit     
  bool bit = this->pin_.input_pin_->digital_read();

  // Minimum 60us slot     
  uint32_t now = micros();
  if (now - start < 60)  
      delayMicroseconds(60 - (now - start));

  return bit;
}

void IRAM_ATTR ESPOneWire::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit(bool((1u << i) & val));
  }
}

void IRAM_ATTR ESPOneWire::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit(bool((1ULL << i) & val));
  }
}

uint8_t IRAM_ATTR ESPOneWire::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit()) << i);
  }
  return ret;
}
uint64_t IRAM_ATTR ESPOneWire::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit()) << i);
  }
  return ret;
}
void IRAM_ATTR ESPOneWire::select(uint64_t address) {
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
}
void IRAM_ATTR ESPOneWire::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->rom_number_ = 0;
}
uint64_t IRAM_ATTR ESPOneWire::search() {
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
std::vector<uint64_t> ESPOneWire::search_vec() {
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}
void IRAM_ATTR ESPOneWire::skip() {
  this->write8(0xCC);  // skip ROM
}

uint8_t IRAM_ATTR *ESPOneWire::rom_number8_() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

}  // namespace dallas
}  // namespace esphome
