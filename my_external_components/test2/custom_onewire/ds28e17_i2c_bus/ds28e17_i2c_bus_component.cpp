/*
DS28E17 1-Wire to i2C Bridge
============================

ESPHome Component for Maxim Integrated DS28E17 1-Wire to i2C Bridge
built using
Arduino Library for Maxim Integrated DS28E17 1-Wire to i2C Bridge
Author: podija https://github.com/podija
adapted by: ashp8i https://github.com/ashp8i

Product: https://www.maximintegrated.com/en/products/interface/controllers-expanders/DS28E17.html

DS28E17I2CBusReadBuffer and DS28E17I2CBusWriteBuffer classes have been created to store buffers for 
read and write operations that apply high-level functions, such as write, memoryWrite, read, and 
memoryRead. These functions will process the I2C address and prepare the commands to send using the
low-level _writeTo and _readFrom functions.

MIT License
*/

#include "custom_onewire.h"
#include "ds28e17_i2c_bus_component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ds28e17_i2c_bus {

static const char *const TAG = "DS28E17";

void DS28E17I2CBusComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DS28E17I2CBusComponent...");
  // Initialize the DS28E17 object with the OneWire object
  ds28e17_ = esphome::ds28e17::DS28E17(*one_wire_);

  // Call the search_vec() method in the CustomOneWire component and get the discovered devices
  std::vector<uint64_t> raw_devices = one_wire_->search_vec();

  for (auto &address : raw_devices) {
    auto *address8 = reinterpret_cast<uint8_t *>(&address);
    if (crc8(address8, 7) != address8[7]) {
      ESP_LOGW(TAG, "1-Wire device 0x%s has invalid CRC.", format_hex(address).c_str());
      continue;
    }
    if (address8[0] != 0x19) { // DS28E17 family code is 0x19
      ESP_LOGW(TAG, "Unknown device type 0x%02X.", address8[0]);
      continue;
    }
    this->found_devices_.push_back(address);
  }
  // Call the configure_i2c_speed method after initializing the DS28E17 object
  configure_i2c_speed();
}

bool DS28E17I2CBusComponent::write_configuration(uint8_t config_byte) {
  // Send the Write Configuration command (0xD2) followed by the configuration byte
  // The command format is: CMD, CONFIG_BYTE, ~CONFIG_BYTE
  uint8_t command_buffer[3] = {0xD2, config_byte, static_cast<uint8_t>(~config_byte)};

  // Assuming the writeBytes method sends the command to the DS28E17 device
  bool success = writeBytes(command_buffer, 3);

  if (!success) {
    // Log an error if the write operation fails
    ESP_LOGE(TAG, "Failed to write configuration byte to DS28E17.");
  }

  return success;
}

void DS28E17I2CBusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DS28E17I2CBusComponent:");
  LOG_UPDATE_INTERVAL(this);

  if (this->found_devices_.empty()) {
    ESP_LOGW(TAG, "  Found no devices!");
  } else {
    ESP_LOGD(TAG, "  Found devices:");
    for (auto &address : this->found_devices_) {
      ESP_LOGD(TAG, "    0x%s", format_hex(address).c_str());
    }
  }

  for (auto *device : this->devices_) {
    LOG_DEVICE("  ", "Device", device);
    if (device->get_index().has_value()) {
      ESP_LOGCONFIG(TAG, "    Index %u", *device->get_index());
      if (*device->get_index() >= this->found_devices_.size()) {
        ESP_LOGE(TAG, "Couldn't find device by index - not connected. Proceeding without it.");
        continue;
      }
    }
    ESP_LOGCONFIG(TAG, "    Address: %s", device->get_address_name().c_str());
  }
}

void DS28E17I2CBusComponent::register_device(DS28E17I2CBus *device) {
  this->devices_.push_back(device);
}

void DS28E17I2CBusComponent::update() {
  // Iterate through the devices connected to the custom OneWire bus
  for (auto *device : devices_) {
    // Make sure the device pointer is not nullptr
    if (device != nullptr) {
      // Begin the transaction for the device
      if (this->begin_transmission_(device->get_address())) {
        // Call the device's update method
        device->update();

        // End the transaction for the device
        this->end_transmission_();
      } else {
        ESP_LOGW(TAG, "Failed to begin transmission for device with address 0x%02X", device->get_address());
      }
    }
  }
}

DS28E17I2CBusComponent::DS28E17I2CBusComponent() {
  this->update_state_ = IDLE;
}

void DS28E17I2CBusComponent::update_non_blocking() {
  static size_t device_index = 0;

  switch (this->update_state_) {
    case IDLE:
      device_index = 0;
      this->update_state_ = BEGIN_TRANSMISSION;
      break;

    case BEGIN_TRANSMISSION:
      if (device_index < devices_.size()) {
        auto *device = devices_[device_index];
        if (device != nullptr) {
          if (this->begin_transmission_(device->get_address())) {
            this->update_state_ = UPDATE_DEVICE;
          } else {
            ESP_LOGW(TAG, "Failed to begin transmission for device with address 0x%02X", device->get_address());
            device_index++;
          }
        } else {
          device_index++;
        }
      } else {
        this->update_state_ = IDLE;
      }
      break;

    case UPDATE_DEVICE:
      devices_[device_index]->update_non_blocking();
      this->update_state_ = END_TRANSMISSION;
      break;

    case END_TRANSMISSION:
      this->end_transmission_();
      device_index++;
      this->update_state_ = BEGIN_TRANSMISSION;
      break;
  }

  yield();
}

void DS28E17_CustomOneWireBus::set_address(uint64_t address) {
  this->address_ = address;
}

optional<uint8_t> DS28E17_CustomOneWireBus::get_index() const {
  return this->index_;
}

void DS28E17_CustomOneWireBus::set_index(uint8_t index) {
  this->index_ = index;
}

uint8_t *DS28E17_CustomOneWireBus::get_address8() {
  return reinterpret_cast<uint8_t *>(&this->address_);
}

const std::string &DS28E17_CustomOneWireBus::get_address_name() {
  if (this->address_name_.empty()) {
    this->address_name_ = std::string("0x") + format_hex(this->address_);
  }

  return this->address_name_;
}

std::string DS28E17_CustomOneWireBus::unique_id() {
  return "ds28e17-" + str_lower_case(format_hex(this->address_));
}

/* readI2C*/

esphome::ErrorCode DS28E17I2CBusComponent::readMultiple(uint8_t address, DS28E17I2CBusReadBuffer *buffers, size_t cnt) {
  if (!initialized_) {
    ESP_LOGVV(TAG, "i2c bus not initialized!");
    return esphome::ERROR_NOT_INITIALIZED;
  }

  for (size_t i = 0; i < cnt; i++) {
    const auto &buf = buffers[i];
    // Call read on parent_ instance
    if (!parent_->read(address, buf.get_data(), buf.get_len())) { // Use the read method from DS28E17I2CBus
      ESP_LOGVV(TAG, "RX %u from %02X failed", buf.get_len(), address);
      return esphome::ERROR_TIMEOUT;
    }
  }

  #ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    char debug_buf[4];
    std::string debug_hex;

    for (size_t i = 0; i < cnt; i++) {
      const auto &buf = buffers[i];
      for (size_t j = 0; j < buf.get_len(); j++) {
        snprintf(debug_buf, sizeof(debug_buf), "%02X", buf.get_data()[j]);
        debug_hex += debug_buf;
      }
    }
    ESP_LOGVV(TAG, "0x%02X RX %s", address, debug_hex.c_str());
  #endif

  return esphome::ERROR_OK;
}

/* writeI2C*/

esphome::ErrorCode DS28E17I2CBusComponent::writeMultiple(uint8_t address, DS28E17I2CBusWriteBuffer *buffers, size_t cnt) {
  if (!initialized_) {
    ESP_LOGVV(TAG, "i2c bus not initialized!");
    return esphome::ERROR_NOT_INITIALIZED;
  }

  for (size_t i = 0; i < cnt; i++) {
    const auto &buf = buffers[i];
    // Call write on parent_ instance
    if (!parent_->write(address, buf.get_data(), buf.get_len())) { // Use the write method from DS28E17I2CBus
      ESP_LOGVV(TAG, "TX %u to %02X failed", buf.get_len(), address);
      return esphome::ERROR_TIMEOUT;
    }
  }

  #ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    char debug_buf[4];
    std::string debug_hex;

    for (size_t i = 0; i < cnt; i++) {
      const auto &buf = buffers[i];
      for (size_t j = 0; j < buf.get_len(); j++) {
        snprintf(debug_buf, sizeof(debug_buf), "%02X", buf.get_data()[j]);
        debug_hex += debug_buf;
      }
    }
    ESP_LOGVV(TAG, "0x%02X TX %s", address, debug_hex.c_str());
  #endif

  return esphome::ERROR_OK;
}

/* readwriteI2C*/

esphome::ErrorCode DS28E17I2CBusComponent::readwriteMultiple(uint8_t address,
                                                    DS28E17I2CBusWriteBuffer *write_buffers, size_t write_cnt,
                                                    DS28E17I2CBusReadBuffer *read_buffers, size_t read_cnt) {
  if (!initialized_) {
    ESP_LOGVV(TAG, "i2c bus not initialized!");
    return esphome::ERROR_NOT_INITIALIZED;
  }

  // Write operation
  for (size_t i = 0; i < write_cnt; i++) {
    const auto &buf = write_buffers[i];
    // Call write on parent_ instance
    if (!parent_->write(address, buf.get_data(), buf.get_len())) {
      ESP_LOGVV(TAG, "TX %u to %02X failed", buf.get_len(), address);
      return esphome::ERROR_TIMEOUT;
    }
  }

  // Read operation
  for (size_t i = 0; i < read_cnt; i++) {
    const auto &buf = read_buffers[i];
    // Call read on parent_ instance
    if (!parent_->read(address, buf.get_data(), buf.get_len())) {
      ESP_LOGVV(TAG, "RX %u from %02X failed", buf.get_len(), address);
      return esphome::ERROR_TIMEOUT;
    }
  }

  #ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    char debug_buf[4];
    std::string debug_hex;

    // Write debug logs
    for (size_t i = 0; i < write_cnt; i++) {
      const auto &buf = write_buffers[i];
      for (size_t j = 0; j < buf.get_len(); j++) {
        snprintf(debug_buf, sizeof(debug_buf), "%02X", buf.get_data()[j]);
        debug_hex += debug_buf;
      }
    }
    ESP_LOGVV(TAG, "0x%02X TX %s", address, debug_hex.c_str());

    // Reset debug_hex for read logs
    debug_hex.clear();

    // Read debug logs
    for (size_t i = 0; i < read_cnt; i++) {
      const auto &buf = read_buffers[i];
      for (size_t j = 0; j < buf.get_len(); j++) {
        snprintf(debug_buf, sizeof(debug_buf), "%02X", buf.get_data()[j]);
        debug_hex += debug_buf;
      }
    }
    ESP_LOGVV(TAG, "0x%02X RX %s", address, debug_hex.c_str());
  #endif

  return esphome::ERROR_OK;
}

}  // namespace ds28e17_i2c_bus
}  // namespace esphome