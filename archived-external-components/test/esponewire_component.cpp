#include "esponewire_component.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstdint>

namespace esphome {
namespace onewire {

static const char *const TAG = "dallas_maxim.one_wire";

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const int ONE_WIRE_ROM_SEARCH = 0xF0;

ESPOneWireComponent::ESPOneWireComponent(InternalGPIOPin *pin) : pin_(pin), isr_pin_(pin) {}

void IRAM_ATTR ESPOneWireComponent::register_callback(std::function<void(uint64_t)> callback) {
  callbacks_.push_back(callback);
}

bool HOT IRAM_ATTR ESPOneWireComponent::reset() {
  isr_pin_.digital_write(false);
  delayMicroseconds(480);
  isr_pin_.digital_write(true);
  delayMicroseconds(70);
  bool presence = !isr_pin_.digital_read();
  delayMicroseconds(410);
  return presence;
}

void IRAM_ATTR ESPOneWireComponent::write_bit(bool bit) {
  isr_pin_.digital_write(false);
  delayMicroseconds(bit ? 10 : 60);
  isr_pin_.digital_write(true);
  delayMicroseconds(bit ? 60 : 10);
}

bool ESPOneWireComponent::read_bit() {
  isr_pin_.digital_write(false);
  delayMicroseconds(10);
  isr_pin_.digital_write(true);
  delayMicroseconds(10);
  bool bit = isr_pin_.digital_read();
  delayMicroseconds(50);
  return bit;
}

void IRAM_ATTR ESPOneWireComponent::write8(uint8_t val) {
  for (int i = 0; i < 8; i++) {
    write_bit((val & (1 << i)) != 0);
  }
}

void IRAM_ATTR ESPOneWireComponent::write64(uint64_t val) {
  for (int i = 0; i < 64; i++) {
    write_bit((val & (1ULL << i)) != 0);
  }
}

// void IRAM_ATTR ESPOneWireComponent::skip() {
//   write8(ONE_WIRE_ROM_SELECT);
// }

uint8_t ESPOneWireComponent::read8() {
  uint8_t val = 0;
  for (int i = 0; i < 8; i++) {
    if (read_bit()) {
      val |= (1 << i);
    }
  }
  return val;
}

uint64_t ESPOneWireComponent::read64() {
  uint64_t val = 0;
  for (int i = 0; i < 64; i++) {
    if (read_bit()) {
      val |= (1ULL << i);
    }
  }
  return val;
}

void IRAM_ATTR ESPOneWireComponent::select(uint64_t address) {
  write8(ONE_WIRE_ROM_SELECT);
  write64(address);
}

// void IRAM_ATTR ESPOneWireComponent::reset_search() {
//   last_discrepancy_ = 0;
//   last_device_flag_ = false;
//   rom_number_ = 0;
// }

// uint64_t ESPOneWireComponent::search() {
//   uint64_t address = 0;
//   int discrepancy = 0;
//   bool reset_search = false;
//   if (last_device_flag_) {
//     reset_search = true;
//     last_device_flag_ = false;
//   }
//   if (reset_search || !reset()) {
//     reset_search();
//     return 0;
//   }
//   write8(ONE_WIRE_ROM_SEARCH);
//   for (int i = 0; i < 64; i++) {
//     int bit = read_bit();
//     int complement = read_bit();
//     if (bit && complement) {
//       break;
//     }
//     if (bit != complement) {
//       discrepancy = i;
//     }
//     if (discrepancy < last_discrepancy_) {
//       if ((rom_number_ & (1ULL << discrepancy)) == 0) {
//         bit = 1;
//       }
//     }
//     if (bit) {
//       rom_number_ |= (1ULL << discrepancy);
//     } else {
//       rom_number_ &= ~(1ULL << discrepancy);
//     }
//     write_bit(bit);
//   }
//   last_discrepancy_ = discrepancy;
//   last_device_flag_ = (discrepancy == 0);
//   if (last_device_flag_) {
//     found_devices_.push_back(rom_number_);
//     address = rom_number_;
//     rom_number_ = 0;
//   }
//   return address;
// }

// std::vector<uint64_t> ESPOneWireComponent::search_vec() {
//   std::vector<uint64_t> addresses;
//   while (true) {
//     uint64_t address = search();
//     if (address == 0) {
//       break;
//     }
//     addresses.push_back(address);
//   }
//   return addresses;
// }

void IRAM_ATTR ESPOneWireComponent::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->rom_number_ = 0;
}

uint64_t IRAM_ATTR ESPOneWireComponent::search() {
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

std::vector<uint64_t> ESPOneWireComponent::search_vec() {
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}

void IRAM_ATTR ESPOneWireComponent::skip() {
  this->write8(0xCC);  // skip ROM
}

// void IRAM_ATTR ESPOneWireComponent::setup() {
//   ESP_LOGCONFIG(TAG, "Setting up ESPOneWireComponent...");
//   isr_pin_.setup();
//   reset_search();
// }

void ESPOneWireComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESPOneWireComponent...");

  // ... code related to pin setup and initialization ...
  pin_->setup();

  // clear bus with 480µs high, otherwise initial reset in search_vec() fails
  pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(480);

  // Create an instance of the OneWireComponent class
  one_wire_ = new ESPOneWireComponent(pin_);  // NOLINT(cppcoreguidelines-owning-memory)

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

void IRAM_ATTR ESPOneWireComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ESPOneWireComponent:");
  LOG_PIN("  Pin: ", pin_);
}

void IRAM_ATTR ESPOneWireComponent::update() {
  std::vector<uint64_t> addresses = search_vec();
  for (uint64_t address : addresses) {
    for (auto &callback : callbacks_) {
      callback(address);
    }
  }
}

}  // namespace onewire
}  // namespace esphome
