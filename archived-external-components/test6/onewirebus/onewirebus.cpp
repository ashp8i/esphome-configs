#include "onewirebus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>

namespace esphome {
namespace onewirebus {

static const char *const TAG = "onewirebus.one_wire";

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const uint8_t ONE_WIRE_ROM_SEARCH = 0xF0;
const uint64_t ONE_WIRE_SEARCH_ERROR = 0xFFFFFFFFFFFFFFFFULL;

using std::vector;

OneWireBus::OneWireBus(InternalGPIOPin *pin) : pin_(pin->to_isr()), failed_communication_attempts_(0) {}

bool HOT IRAM_ATTR OneWireBus::reset() {
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html

  // Wait for communication to clear (delay G)
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  for (int retries = 10; retries > 0; --retries) {
    if (pin_.digital_read()) {
      break;
    }
    delayMicroseconds(2);
  }

  // Send 480µs LOW TX reset pulse (drive bus low, delay H)
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);
  delayMicroseconds(480);

  // Release the bus, delay I
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(70);

  // Sample bus, 0=device(s) present, 1=no device present
  bool presence_detected = !pin_.digital_read();
  // Delay J
  delayMicroseconds(410);

  if (!presence_detected) {
    failed_communication_attempts_--;
    ESP_LOGD(TAG, "Presence pulse succeeded in reset");
  } else {
    failed_communication_attempts_++;
    ESP_LOGD(TAG, "Presence pulse failed in reset");
  }

  return presence_detected;
}



bool HOT IRAM_ATTR OneWireBus::write_bit(bool bit_to_write) { 
  // Drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);

  // from datasheet
  // write 0 low time: t_low0: min=60µs, max=120µs
  // write 1 low time: t_low1: min=1µs, max=15µs
  // time slot: t_slot: min=60µs, max=120µs
  // recovery time: t_rec: min=1µs
  // ds18b20 appears to read the bus after roughly 14µs
  const uint32_t delay0 = bit_to_write ? 6 : 60;      
  const uint32_t delay1 = bit_to_write ? 54 : 5;

  // Delay A/C
  delayMicroseconds(delay0);
  // Release bus
  pin_.digital_write(true);
  // Delay B/D
  delayMicroseconds(delay1);
  // Add a small delay to allow the slave to process the bit
  delayMicroseconds(1);

  bool read_back = this->read_bit();
  if (read_back != bit_to_write) {  
    this->reset();          
    ESP_LOGD(TAG, "Write bit failed");     
  }

  return read_back;
}

bool HOT IRAM_ATTR OneWireBus::read_bit() {  
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

  // Release bus, delay E
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  // Unfortunately some frameworks have different characteristics than others
  // esp32 arduino appears to pull the bus low only after the digital_write(false),
  // whereas on esp-idf it already happens during the pin_mode(OUTPUT)
  // manually correct for this with these constants.

#ifdef USE_ESP32
  uint32_t timing_constant = 12;
#else
  const uint32_t timing_constant = 14;
#endif

  // measure from start value directly, to get best accurate timing no matter
  // how long pin_mode/delayMicroseconds took
  while (micros() - start < timing_constant) {}

  // sample bus to read bit from peer
  bool bit = pin_.digital_read();

  // read slot is at least 60µs; get as close to 60µs to spend less time with interrupts locked
  uint32_t const now = micros();
  if (now - start < 60) {
    delayMicroseconds(60 - (now - start));
  }

  return bit;
}

void OneWireBus::log_communication_status() {
  if (failed_communication_attempts_ > 0) {
    ESP_LOGW(TAG, "Communication failed %d times", failed_communication_attempts_);
    failed_communication_attempts_ = 0;  // Reset counter
  } else {
    ESP_LOGI(TAG, "Communication successful");
  }
}

void IRAM_ATTR OneWireBus::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    if (!this->write_bit(bool((1u << i) & val))) {
      failed_communication_attempts_++;
      ESP_LOGD(TAG, "Write byte failed");
    }
  }
}

void IRAM_ATTR OneWireBus::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    if (!this->write_bit(bool((1ULL << i) & val))) {
      failed_communication_attempts_++;
      ESP_LOGD(TAG, "Write 64-bit value failed"); 
    }
  }
}

uint8_t IRAM_ATTR OneWireBus::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    bool bit = this->read_bit();
    if (bit == false) {
      ESP_LOGW(TAG, "Failed to read byte");
      break;
    }
    ret |= (uint8_t(bit) << i);
  }
  return ret;
}

uint64_t IRAM_ATTR OneWireBus::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 64; i++) {
    bool bit = this->read_bit();
    if (bit == false) {
      ESP_LOGW(TAG, "Failed to read 64-bit value");
      break;
    }
    ret |= (uint64_t(bit) << i);
  }
  return ret;
}

void IRAM_ATTR OneWireBus::select(uint64_t address) {
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
}
void IRAM_ATTR OneWireBus::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  for (auto &elem : this->rom_number_) {
    elem = 0;
  }
}
uint64_t IRAM_ATTR OneWireBus::search() {
  const int MAX_RETRIES = 3;
  const int MAX_ERRORS = 5; // Change this value to adjust the error limit
  int retries = 0;
  int errors = 0;

  if (this->last_device_flag_) {
    return 0u;
  }

  bool search_result = false;

  while (retries < MAX_RETRIES) {
    {
      InterruptLock lock;
      if (!this->reset()) {
        // Reset failed or no devices present
        this->reset_search();
        return 0u;
      }

      uint8_t id_bit_number = 1;
      uint8_t last_zero = 0;
      uint8_t rom_byte_index = 0;
      uint8_t rom_bit_mask = 1;

      // Initiate search
      this->write8(ONE_WIRE_ROM_SEARCH);

      while (id_bit_number <= 64) {
        // read bit
        bool id_bit = this->read_bit();
        // read its complement
        bool cmp_id_bit = this->read_bit();

        if (id_bit && cmp_id_bit) {
          // Bus error - abort search and restart
          errors++;
          if (errors > MAX_ERRORS) {
            this->reset_search();
            return ONE_WIRE_SEARCH_ERROR;
          }
          break;
        }

        bool bit_written;
        if (id_bit != cmp_id_bit) {
          // Bits are different, set bit_written to the value of id_bit
          bit_written = id_bit;
        } else {
          // Bits are the same, we need to choose a path
          if (id_bit_number == this->last_discrepancy_) {
            bit_written = true;
          } else if (id_bit_number > this->last_discrepancy_) {
            bit_written = false;
          } else {
            bit_written = (this->rom_number_[rom_byte_index] & rom_bit_mask) != 0;
          }

          if (!bit_written) {
            last_zero = id_bit_number;
          }
        }

        if (bit_written) {
          this->rom_number_[rom_byte_index] |= rom_bit_mask;
        } else {
          this->rom_number_[rom_byte_index] &= ~rom_bit_mask;
        }

        // Write the chosen bit
        this->write_bit(bit_written);

        // Increment bit counters
        id_bit_number++;
        rom_bit_mask <<= 1;
        if (rom_bit_mask == 0) {
          rom_byte_index++;
          rom_bit_mask = 1;
        }
      }

      if (id_bit_number > 64) {
        this->last_discrepancy_ = last_zero;
        if (this->last_discrepancy_ == 0) {
          // we're at root and have no choices left, so this was the last one.
          this->last_device_flag_ = true;
        }
        search_result = true;
        errors = 0; // Reset error counter after successful search
        break;
      }
    }

    // If search successful, break out of retry loop
    if (search_result) {
      break;
    } else {
      retries++;
    }
  }

  if (retries == MAX_RETRIES) {
    return ONE_WIRE_SEARCH_ERROR; // Indicate failed search
  }

  search_result = search_result && std::any_of(std::begin(this->rom_number_), std::end(this->rom_number_), [](uint8_t val) { return val != 0; });
  if (!search_result) {
    this->reset_search();
    return 0u;
  }

  uint64_t result = 0;
  for (int i = 0; i < 8; ++i) {
    result |= static_cast<uint64_t>(this->rom_number_[i]) << (8 * i);
  }
  return result;
}

std::vector<uint64_t> OneWireBus::search_vec() {
  std::vector<uint64_t> res;  

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u) {
    res.push_back(address);
  }
  
  return res; 
}

void IRAM_ATTR OneWireBus::skip() {
  this->write8(0xCC);  // skip ROM
}

}  // namespace onewirebus
}  // namespace esphome