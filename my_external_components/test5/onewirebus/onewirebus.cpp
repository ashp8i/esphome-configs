#include "onewirebus.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace onewirebus {

static const char *const TAG = "onewirebus";

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const int ONE_WIRE_ROM_SEARCH = 0xF0;

// OneWireBus::OneWireBus(InternalGPIOPin *pin) { pin_ = pin->to_isr(); }

OneWireBus::OneWireBus(InternalGPIOPin *pin) { 
  // split_io_ = false;
  pin_ = pin->to_isr(); 
}

OneWireBus::OneWireBus(InternalGPIOPin *input_pin, InternalGPIOPin *output_pin) {
  split_io_ = true;
  input_pin_ = input_pin->to_isr();
  output_pin_ = output_pin->to_isr();
} 

bool HOT IRAM_ATTR OneWireBus::reset() {
  if (split_io_) {
  // Use separate input and output pins
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  // Wait for communication to clear (delay G) set high
  // Set input pin to input with pullup
  output_pin_.digital_write(true);
  input_pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  // input_pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  // output_pin_.pin_mode(gpio::FLAG_INPUT);
  uint8_t retries = 125;
  do {
    if (--retries == 0)
      return false;
    delayMicroseconds(2);
  } while (!input_pin_.digital_read());

  // Send 480µs LOW TX reset pulse (drive bus low, delay H)
  output_pin_.digital_write(false);
  output_pin_.pin_mode(gpio::FLAG_OUTPUT);
  delayMicroseconds(480);
  // output_pin_.digital_write(true); // Remove this line

  // Release the bus, delay I
  output_pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(70);

  // sample bus, 0=device(s) present, 1=no device present
  bool present = !input_pin_.digital_read();
  // delay J
  delayMicroseconds(410);
  return present;
  } else {
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
}

void HOT IRAM_ATTR OneWireBus::write_bit(bool bit) {
  if (split_io_) {
  // Use separate input and output pins
  // Drive bus low
  output_pin_.pin_mode(gpio::FLAG_OUTPUT);
  output_pin_.digital_write(false);
  
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
  output_pin_.digital_write(true);
  // output_pin_.pin_mode(gpio::FLAG_INPUT);
  // delay B/D
  delayMicroseconds(delay1);
  } else {
  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);

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
  pin_.digital_write(true);
  // delay B/D
  delayMicroseconds(delay1);
  }
}

bool HOT IRAM_ATTR OneWireBus::read_bit() {
  if (split_io_) {
  // Use separate input and output pins
  // drive bus low
  output_pin_.pin_mode(gpio::FLAG_OUTPUT);
  output_pin_.digital_write(false);

  // note: for reading we'll need very accurate timing, as the
  // timing for the digital_read() is tight; according to the datasheet,
  // we should read at the end of 16µs starting from the bus low
  // typically, the ds18b20 pulls the line high after 11µs for a logical 1
  // and 29µs for a logical 0

  uint32_t start = micros();
  // datasheet says >1µs
  delayMicroseconds(3);

  // release bus, delay E
  // output_pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  // input_pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

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
  bool bit = input_pin_.digital_read();

  // read slot is at least 60µs; get as close to 60µs to spend less time with interrupts locked
  uint32_t now = micros();
  if (now - start < 60)
    delayMicroseconds(60 - (now - start));

  return bit;
  } else {
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
}

void IRAM_ATTR OneWireBus::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit(bool((1u << i) & val));
  }
}

void IRAM_ATTR OneWireBus::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit(bool((1ULL << i) & val));
  }
}

uint8_t IRAM_ATTR OneWireBus::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit()) << i);
  }
  return ret;
}
uint64_t IRAM_ATTR OneWireBus::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit()) << i);
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
  this->rom_number_ = 0;
}
uint64_t IRAM_ATTR OneWireBus::search() {
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
std::vector<uint64_t> OneWireBus::search_vec() {
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}

void IRAM_ATTR OneWireBus::skip() {
  this->write8(0xCC);  // skip ROM
}

uint8_t IRAM_ATTR *OneWireBus::rom_number8_() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

static const uint8_t ONE_WIRE_MODEL_DS18S20 = 0x10;
static const uint8_t ONE_WIRE_MODEL_DS1822 = 0x22;
static const uint8_t ONE_WIRE_MODEL_DS18B20 = 0x28;
static const uint8_t ONE_WIRE_MODEL_DS1825 = 0x3B;
static const uint8_t ONE_WIRE_MODEL_DS28EA00 = 0x42;
static const uint8_t ONE_WIRE_COMMAND_START_CONVERSION = 0x44;
static const uint8_t ONE_WIRE_COMMAND_READ_SCRATCH_PAD = 0xBE;
static const uint8_t ONE_WIRE_COMMAND_WRITE_SCRATCH_PAD = 0x4E;

uint16_t OneWireTemperatureSensor::millis_to_wait_for_conversion() const {
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

void OneWireBusComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up OneWireBusComponent...");

  split_io_ = false; // Add this line
  
  if (split_io_) {
  input_pin_->setup();
  output_pin_->setup();
  // clear bus with 480μs high, otherwise initial reset in search_vec() fails
  input_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(480);
  one_wire_ = new OneWireBus(input_pin_, output_pin_);
  } else {
  pin_->setup();
  
  // clear bus with 480µs high, otherwise initial reset in search_vec() fails
  pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(480);

  one_wire_ = new OneWireBus(pin_);  // NOLINT(cppcoreguidelines-owning-memory) 
  }

  std::vector<uint64_t> raw_sensors;
  raw_sensors = this->one_wire_->search_vec();

  for (auto &address : raw_sensors) {
    auto *address8 = reinterpret_cast<uint8_t *>(&address);
    if (crc8(address8, 7) != address8[7]) {
      ESP_LOGW(TAG, "1-Wire device 0x%s has invalid CRC.", format_hex(address).c_str());
      continue;
    }
    if (address8[0] != ONE_WIRE_MODEL_DS18S20 && address8[0] != ONE_WIRE_MODEL_DS1822 &&
        address8[0] != ONE_WIRE_MODEL_DS18B20 && address8[0] != ONE_WIRE_MODEL_DS1825 &&
        address8[0] != ONE_WIRE_MODEL_DS28EA00) {
      ESP_LOGW(TAG, "Unknown device type 0x%02X.", address8[0]);
      continue;
    }
    this->found_sensors_.push_back(address);
  }

  for (auto *sensor : this->sensors_) {
    if (sensor->get_index().has_value()) {
      if (*sensor->get_index() >= this->found_sensors_.size()) {
        this->status_set_error();
        continue;
      }
      sensor->set_address(this->found_sensors_[*sensor->get_index()]);
    }

    if (!sensor->setup_sensor()) {
      this->status_set_error();
    }
  }
}
void OneWireBusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "OneWireBusComponent:");
  
  if (split_io_) {
    LOG_PIN("  Input Pin: ", this->input_pin_);
    LOG_PIN("  Output Pin: ", this->output_pin_);
  } else {
    LOG_PIN("  Pin: ", this->pin_);
  }

  LOG_UPDATE_INTERVAL(this);

  if (this->found_sensors_.empty()) {
    ESP_LOGW(TAG, "  Found no sensors!");
  } else {
    ESP_LOGD(TAG, "  Found sensors:");
    for (auto &address : this->found_sensors_) {
      ESP_LOGD(TAG, "    0x%s", format_hex(address).c_str());
    }
  }

  for (auto *sensor : this->sensors_) {
    LOG_SENSOR("  ", "Device", sensor);
    if (sensor->get_index().has_value()) {
      ESP_LOGCONFIG(TAG, "    Index %u", *sensor->get_index());
      if (*sensor->get_index() >= this->found_sensors_.size()) {
        ESP_LOGE(TAG, "Couldn't find sensor by index - not connected. Proceeding without it.");
        continue;
      }
    }
    ESP_LOGCONFIG(TAG, "    Address: %s", sensor->get_address_name().c_str());
    ESP_LOGCONFIG(TAG, "    Resolution: %u", sensor->get_resolution());
  }
}

void OneWireBusComponent::register_sensor(OneWireTemperatureSensor *sensor) { this->sensors_.push_back(sensor); }
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
    for (auto *sensor : this->sensors_) {
      sensor->publish_state(NAN);
    }
    return;
  }

  {
    InterruptLock lock;
    this->one_wire_->skip();
    this->one_wire_->write8(ONE_WIRE_COMMAND_START_CONVERSION);
  }

  for (auto *sensor : this->sensors_) {
    this->set_timeout(sensor->get_address_name(), sensor->millis_to_wait_for_conversion(), [this, sensor] {
      bool res = sensor->read_scratch_pad();

      if (!res) {
        ESP_LOGW(TAG, "'%s' - Resetting bus for read failed!", sensor->get_name().c_str());
        sensor->publish_state(NAN);
        this->status_set_warning();
        return;
      }
      if (!sensor->check_scratch_pad()) {
        sensor->publish_state(NAN);
        this->status_set_warning();
        return;
      }

      float tempc = sensor->get_temp_c();
      ESP_LOGD(TAG, "'%s': Got Temperature=%.1f°C", sensor->get_name().c_str(), tempc);
      sensor->publish_state(tempc);
    });
  }
}

void OneWireTemperatureSensor::set_address(uint64_t address) { this->address_ = address; }
uint8_t OneWireTemperatureSensor::get_resolution() const { return this->resolution_; }
void OneWireTemperatureSensor::set_resolution(uint8_t resolution) { this->resolution_ = resolution; }
optional<uint8_t> OneWireTemperatureSensor::get_index() const { return this->index_; }
void OneWireTemperatureSensor::set_index(uint8_t index) { this->index_ = index; }
uint8_t *OneWireTemperatureSensor::get_address8() { return reinterpret_cast<uint8_t *>(&this->address_); }
const std::string &OneWireTemperatureSensor::get_address_name() {
  if (this->address_name_.empty()) {
    this->address_name_ = std::string("0x") + format_hex(this->address_);
  }

  return this->address_name_;
}
bool IRAM_ATTR OneWireTemperatureSensor::read_scratch_pad() {
  auto *wire = this->parent_->one_wire_;

  {
    InterruptLock lock;

    if (!wire->reset()) {
      return false;
    }
  }

  {
    InterruptLock lock;

    wire->select(this->address_);
    wire->write8(ONE_WIRE_COMMAND_READ_SCRATCH_PAD);

    for (unsigned char &rspd : this->scratch_pad_) {
      rspd = wire->read8();
    }
  }

  return true;
}
bool OneWireTemperatureSensor::setup_sensor() {
  bool const rsp = this->read_scratch_pad();

  if (!rsp) {
    ESP_LOGE(TAG, "Reading scratchpad failed: reset");
    return false;
  }
  if (!this->check_scratch_pad())
    return false;

  if (this->scratch_pad_[4] == this->resolution_)
    return false;

  if (this->get_address8()[0] == ONE_WIRE_MODEL_DS18S20) {
    // DS18S20 doesn't support resolution.
    ESP_LOGW(TAG, "DS18S20 doesn't support setting resolution.");
    return false;
  }

  switch (this->resolution_) {
    case 12:
      this->scratch_pad_[4] = 0x7F;
      break;
    case 11:
      this->scratch_pad_[4] = 0x5F;
      break;
    case 10:
      this->scratch_pad_[4] = 0x3F;
      break;
    case 9:
    default:
      this->scratch_pad_[4] = 0x1F;
      break;
  }

  auto *wire = this->parent_->one_wire_;
  {
    InterruptLock lock;
    if (wire->reset()) {
      wire->select(this->address_);
      wire->write8(ONE_WIRE_COMMAND_WRITE_SCRATCH_PAD);
      wire->write8(this->scratch_pad_[2]);  // high alarm temp
      wire->write8(this->scratch_pad_[3]);  // low alarm temp
      wire->write8(this->scratch_pad_[4]);  // resolution
      wire->reset();

      // write value to EEPROM
      wire->select(this->address_);
      wire->write8(0x48);
    }
  }

  delay(20);  // allow it to finish operation
  wire->reset();
  return true;
}
bool OneWireTemperatureSensor::check_scratch_pad() {
  bool chksum_validity = (crc8(this->scratch_pad_, 8) == this->scratch_pad_[8]);
  bool config_validity = false;

  switch (this->get_address8()[0]) {
    case ONE_WIRE_MODEL_DS18B20:
      config_validity = ((this->scratch_pad_[4] & 0x9F) == 0x1F);
      break;
    default:
      config_validity = ((this->scratch_pad_[4] & 0x10) == 0x10);
  }

#ifdef ESPHOME_LOG_LEVEL_VERY_VERBOSE
  ESP_LOGVV(TAG, "Scratch pad: %02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X (%02X)", this->scratch_pad_[0],
            this->scratch_pad_[1], this->scratch_pad_[2], this->scratch_pad_[3], this->scratch_pad_[4],
            this->scratch_pad_[5], this->scratch_pad_[6], this->scratch_pad_[7], this->scratch_pad_[8],
            crc8(this->scratch_pad_, 8));
#endif
  if (!chksum_validity) {
    ESP_LOGW(TAG, "'%s' - Scratch pad checksum invalid!", this->get_name().c_str());
  } else if (!config_validity) {
    ESP_LOGW(TAG, "'%s' - Scratch pad config register invalid!", this->get_name().c_str());
  }
  return chksum_validity && config_validity;
}
float OneWireTemperatureSensor::get_temp_c() {
  int16_t temp = (int16_t(this->scratch_pad_[1]) << 11) | (int16_t(this->scratch_pad_[0]) << 3);
  if (this->get_address8()[0] == ONE_WIRE_MODEL_DS18S20) {
    int diff = (this->scratch_pad_[7] - this->scratch_pad_[6]) << 7;
    temp = ((temp & 0xFFF0) << 3) - 16 + (diff / this->scratch_pad_[7]);
  }

  return temp / 128.0f;
}
std::string OneWireTemperatureSensor::unique_id() { return "onewirebus-" + str_lower_case(format_hex(this->address_)); }

}  // namespace onewirebus
}  // namespace esphome
