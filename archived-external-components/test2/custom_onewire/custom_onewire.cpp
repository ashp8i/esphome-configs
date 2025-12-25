#include "custom_onewire.h"

namespace esphome {
namespace custom_onewire {

static const char *const TAG = "dallas_maxim.one_wire";

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const int ONE_WIRE_ROM_SEARCH = 0xF0;

CustomOneWire::CustomOneWire(InternalGPIOPin *pin, InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, bool low_power_mode, bool overdrive_mode, bool parasitic_power_mode)
    : pin_(pin), in_pin_(in_pin), out_pin_(out_pin), low_power_mode_(low_power_mode), overdrive_mode_(overdrive_mode), parasitic_power_mode_(parasitic_power_mode) {
  
  if (parasitic_power_mode_) {
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
    pin_ = pin->to_isr();
  }
}

bool HOT IRAM_ATTR CustomOneWire::reset() {
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

void HOT IRAM_ATTR CustomOneWire::write_bit(bool bit) {
  // drive bus low
  if (in_pin_ && out_pin_) {
    // Split I/O mode
    in_pin_.pin_mode(gpio::FLAG_OUTPUT);
    out_pin_.pin_mode(gpio::FLAG_OUTPUT);
    in_pin_.digital_write(false);
    out_pin_.digital_write(bit);
  } else {
    // Single-pin mode
    pin_.pin_mode(gpio::FLAG_OUTPUT);
    pin_.digital_write(false);
    pin_.digital_write(bit);
  }

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
  if (in_pin_ && out_pin_) {
    // Split I/O mode
    in_pin_.digital_write(true);
    out_pin_.digital_write(false);
  } else {
    // Single-pin mode
    pin_.digital_write(true);
    pin_.digital_write(false);
  }
  // delay B/D
  delayMicroseconds(delay1);
}

bool HOT IRAM_ATTR CustomOneWire::read_bit() {
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

void IRAM_ATTR CustomOneWire::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit(bool((1u << i) & val));
  }
}

void IRAM_ATTR CustomOneWire::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit(bool((1ULL << i) & val));
  }
}

uint8_t IRAM_ATTR CustomOneWire::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit()) << i);
  }
  return ret;
}
uint64_t IRAM_ATTR CustomOneWire::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit()) << i);
  }
  return ret;
}
void IRAM_ATTR CustomOneWire::select(uint64_t address) {
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
}
void IRAM_ATTR CustomOneWire::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->rom_number_ = 0;
}
uint64_t IRAM_ATTR CustomOneWire::search() {
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
std::vector<uint64_t> CustomOneWire::search_vec() {
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}
void IRAM_ATTR CustomOneWire::skip() {
  this->write8(0xCC);  // skip ROM
}

uint8_t IRAM_ATTR *CustomOneWire::rom_number8_() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

void IRAM_ATTR CustomOneWire::set_low_power() {
  // reduce bus voltage to save power
  pin_.analog_write(0);
}

void IRAM_ATTR CustomOneWire::set_overdrive() {
  // reduce delay times for overdrive mode
  uint32_t delay0 = bit ? 1 : 5;
  uint32_t delay1 = bit ? 4 : 1;
}

OneWireModeTracker one_wire_mode_tracker;

void setup() {
  one_wire_mode_tracker.setup();
}

void loop() {
  // Call the update method of the OneWireModeTracker component
  one_wire_mode_tracker.update();
}

}  // namespace custom_onewire
}  // namespace esphome

