#include "tuya.h"
#include "esphome/components/network/util.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya";
static const int COMMAND_DELAY = 20;
static const int RECEIVE_TIMEOUT = 300;
static const int MAX_RETRIES = 5;
static const int HANDSHAKE_TIMEOUT = 15000;  // 15 seconds for v0 handshake

std::string command_type_to_string(TuyaCommandType cmd) {
  switch (cmd) {
    case TuyaCommandType::HEARTBEAT: return "HEARTBEAT";
    case TuyaCommandType::PRODUCT_QUERY: return "PRODUCT_QUERY";
    case TuyaCommandType::CONF_QUERY: return "CONF_QUERY";
    case TuyaCommandType::WIFI_STATE: return "WIFI_STATE";
    case TuyaCommandType::DATAPOINT_DELIVER: return "DATAPOINT_DELIVER/REPORT_ASYNC";
    case TuyaCommandType::DATAPOINT_REPORT_SYNC: return "DATAPOINT_REPORT_SYNC";
    case TuyaCommandType::DATAPOINT_QUERY: return "DATAPOINT_QUERY";
    case TuyaCommandType::DATAPOINT_REPORT_ACK: return "DATAPOINT_REPORT_ACK";
    case TuyaCommandType::WIFI_TEST: return "WIFI_TEST";
    case TuyaCommandType::WIFI_RSSI: return "WIFI_RSSI";
    case TuyaCommandType::LOCAL_TIME_QUERY: return "LOCAL_TIME_QUERY";
    case TuyaCommandType::GET_NETWORK_STATUS: return "GET_NETWORK_STATUS";
    case TuyaCommandType::EXTENDED_SERVICES: return "EXTENDED_SERVICES";
    case TuyaCommandType::VACUUM_MAP_UPLOAD: return "VACUUM_MAP_UPLOAD";
    default: return "UNKNOWN";
  }
}

void Tuya::setup() {
  this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
  this->init_failed_ = false;
  this->init_retries_ = 0;
  this->protocol_version_ = 0xFF;  // Unknown initially
  this->handshake_start_time_ = 0;

  // Start with heartbeat immediately
  this->send_empty_command_(TuyaCommandType::HEARTBEAT);
  this->handshake_start_time_ = millis();

  // Long-term heartbeat interval
  this->set_interval("heartbeat", 15000, [this] {
    if (this->init_state_ == TuyaInitState::INIT_DONE) {
      this->send_empty_command_(TuyaCommandType::HEARTBEAT);
    }
  });
    // Initialize protocol detection
  this->protocol_detected_ = false;
  this->protocol_version_ = 0xFF;  // Unknown initially
  this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
  this->handshake_start_time_ = millis();

  ESP_LOGI(TAG, "Tuya component initialized - waiting for protocol detection");
}

void Tuya::loop() {
  // CRITICAL: Read UART data FIRST - This is what's missing!
  while (this->available()) {
    uint8_t byte;
    if (this->read_byte(&byte)) {
      this->rx_message_.push_back(byte);
      ESP_LOGD(TAG, "UART BYTE RECEIVED: 0x%02X", byte);  // Debug incoming data
    }
  }

  // Always process incoming frames first
  this->process_frames_();

  // Protocol detection phase
  if (!this->protocol_detected_) {
    this->detect_protocol_version_();

    if (this->protocol_detected_ || this->protocol_version_override_ != 0xFF) {
      this->start_initialization_();
    }
    return;
  }

  // Initialization and operation phase
  if (this->init_state_ != TuyaInitState::INIT_DONE) {
    this->handle_initialization_();
  } else {
    // Normal operation - just process frames
    // Device will send commands when needed
  }
}

void Tuya::start_initialization_() {
  // Set up the appropriate initialization based on protocol
  if (this->protocol_version_ == 3) {
    this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
    this->handshake_start_time_ = millis();
    ESP_LOGI(TAG, "Starting v3 initialization sequence");
  } else {
    this->init_state_ = TuyaInitState::INIT_HEARTBEAT;
    this->handshake_start_time_ = millis();
    ESP_LOGI(TAG, "Starting v0/v1 initialization sequence");
  }
}

void Tuya::handle_initialization_() {
  uint32_t now = millis();
  uint32_t init_duration = now - this->handshake_start_time_;

  // Check for completion timeouts and fallback strategies
  switch (this->init_state_) {
    case TuyaInitState::INIT_HEARTBEAT: {
      // Send heartbeat every 1-2 seconds during init
      if (now - this->last_command_timestamp_ > 1500) {
        this->send_empty_command_(TuyaCommandType::HEARTBEAT);

        // Check for handshake-only devices that need product query
        if (init_duration > 3000 && this->product_.empty()) {
          // Device responded to heartbeat but hasn't sent product data
          // Try sending product query for handshake devices
          ESP_LOGD(TAG, "No product data received, sending product query");
          this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
        }
      }

      // If we get heartbeats but no handshake response after 10 seconds,
      // assume it's a heartbeat-only device
      if (init_duration > 10000 && this->product_.empty()) {
        ESP_LOGI(TAG, "Heartbeat-only device detected - completing init without handshake");
        this->complete_initialization_(TuyaInitType::HEARTBEAT_ONLY);
      }
      break;
    }

    case TuyaInitState::INIT_PRODUCT: {
      // v0/v1 handshake - waiting for product response
      if (now - this->last_command_timestamp_ > 1000) {
        if (init_duration > 5000) {
          // Product query timeout - might be heartbeat-only device
          ESP_LOGD(TAG, "Product query timeout, checking if device is heartbeat-only");
          if (this->has_received_heartbeats_()) {
            ESP_LOGI(TAG, "Heartbeat-only device detected during handshake");
            this->complete_initialization_(TuyaInitType::HEARTBEAT_ONLY);
          } else {
            // Retry product query
            this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
          }
        } else {
          // Retry product query
          this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
        }
      }
      break;
    }

    case TuyaInitState::INIT_CONF: {
      // v0/v1 handshake - waiting for config response
      if (now - this->last_command_timestamp_ > 1000) {
        if (init_duration > 5000) {
          // Config query timeout - complete with partial info
          ESP_LOGI(TAG, "Config query timeout, completing with partial handshake");
          this->complete_initialization_(TuyaInitType::PARTIAL_HANDSHAKE);
        } else {
          this->send_empty_command_(TuyaCommandType::CONF_QUERY);
        }
      }
      break;
    }

    case TuyaInitState::INIT_WIFI_STATUS: {
      // v3 handshake - waiting for WiFi status response
      if (now - this->last_command_timestamp_ > 1000) {
        if (init_duration > 5000) {
          // WiFi status timeout
          ESP_LOGI(TAG, "WiFi status timeout, completing v3 init");
          this->complete_initialization_(TuyaInitType::PARTIAL_HANDSHAKE);
        } else {
          this->send_command_(TuyaCommand{TuyaCommandType::GET_NETWORK_STATUS, {this->get_wifi_status_code_()}});
        }
      }
      break;
    }

    default:
      break;
  }
}

void Tuya::complete_initialization_(TuyaInitType init_type) {
  this->init_state_ = TuyaInitState::INIT_DONE;
  this->init_type_ = init_type;

  switch (init_type) {
    case TuyaInitType::FULL_HANDSHAKE:
      ESP_LOGI(TAG, "Initialization complete: Full handshake");
      break;
    case TuyaInitType::PARTIAL_HANDSHAKE:
      ESP_LOGI(TAG, "Initialization complete: Partial handshake");
      break;
    case TuyaInitType::HEARTBEAT_ONLY:
      ESP_LOGI(TAG, "Initialization complete: Heartbeat-only device");
      break;
  }

  // Set up appropriate heartbeat interval based on device type
  this->setup_heartbeat_interval_();

  // Call completion callbacks
  this->initialized_callback_.call();
  this->dump_config();
}

void Tuya::setup_heartbeat_interval_() {
  // Remove existing heartbeat interval if any
  this->cancel_interval("heartbeat");

  // Set up heartbeat based on device type
  uint32_t interval_ms = 15000; // Default 15 seconds

  switch (this->init_type_) {
    case TuyaInitType::FULL_HANDSHAKE:
      interval_ms = 15000; // Standard devices
      break;
    case TuyaInitType::PARTIAL_HANDSHAKE:
      interval_ms = 30000; // Slower for problematic devices
      break;
    case TuyaInitType::HEARTBEAT_ONLY:
      interval_ms = 15000; // Standard for heartbeat-only
      break;
  }

  // Set up heartbeat interval
  this->set_interval("heartbeat", interval_ms, [this] {
    if (this->init_state_ == TuyaInitState::INIT_DONE) {
      this->send_empty_command_(TuyaCommandType::HEARTBEAT);
    }
  });
}

bool Tuya::has_received_heartbeats_() {
  // Check if we've received any heartbeats in the last 30 seconds
  return (millis() - this->last_heartbeat_time_ < 30000);
}

void Tuya::handle_v0_v1_initialization_() {
  // Your proven v0/v1 initialization sequence
  ESP_LOGI(TAG, "Starting v0/v1 initialization sequence");

  switch (this->init_state_) {
    case TuyaInitState::INIT_HEARTBEAT:
      if (millis() - this->handshake_start_time_ > 1000) {
        ESP_LOGD(TAG, "Sending v0/v1 heartbeat");
        this->send_empty_command_(TuyaCommandType::HEARTBEAT);  // 0x00
        this->handshake_start_time_ = millis();
      }
      break;

    case TuyaInitState::INIT_PRODUCT:
      if (millis() - this->handshake_start_time_ > 1000) {
        ESP_LOGD(TAG, "Sending v0/v1 product query");
        this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
        this->handshake_start_time_ = millis();
      }
      break;

    case TuyaInitState::INIT_CONF:
      if (millis() - this->handshake_start_time_ > 1000) {
        ESP_LOGD(TAG, "Sending v0/v1 config query");
        this->send_empty_command_(TuyaCommandType::CONF_QUERY);
        this->handshake_start_time_ = millis();
      }
      break;

    case TuyaInitState::INIT_DONE:
      ESP_LOGI(TAG, "v0/v1 initialization complete");
      this->protocol_detected_ = true;
      break;
  }
}

void Tuya::handle_v3_initialization_() {
  // V3 initialization sequence for learning
  ESP_LOGI(TAG, "Starting v3 initialization sequence");

  switch (this->init_state_) {
    case TuyaInitState::INIT_HEARTBEAT:
      if (millis() - this->handshake_start_time_ > 1000) {
        ESP_LOGD(TAG, "Sending v3 heartbeat");
        this->send_command_(TuyaCommand{.cmd = TuyaCommandType::HEARTBEAT});  // Will use 0x1C
        this->handshake_start_time_ = millis();
      }
      break;

    case TuyaInitState::INIT_PRODUCT:
      if (millis() - this->handshake_start_time_ > 1000) {
        ESP_LOGD(TAG, "Sending v3 product query");
        this->send_command_(TuyaCommand{.cmd = TuyaCommandType::PRODUCT_QUERY});
        this->handshake_start_time_ = millis();
      }
      break;

    case TuyaInitState::INIT_CONF:
      if (millis() - this->handshake_start_time_ > 1000) {
        ESP_LOGD(TAG, "Sending v3 network status request");
        this->send_command_(TuyaCommand{.cmd = TuyaCommandType::GET_NETWORK_STATUS});
        this->handshake_start_time_ = millis();
      }
      break;

    case TuyaInitState::INIT_DONE:
      ESP_LOGI(TAG, "v3 initialization complete");
      this->protocol_detected_ = true;
      break;
  }
}

void Tuya::detect_protocol_version_() {
  // Auto-detect based on what the device actually sends
  if (this->protocol_version_ != 0xFF) {
    return; // Already detected
  }

  // Look for v3 heartbeats (0x1C) - indicates v3 device
  if (this->rx_message_.size() >= 7) {
    for (size_t i = 0; i < this->rx_message_.size() - 6; i++) {
      if (this->rx_message_[i] == 0x55 &&
          this->rx_message_[i+1] == 0xAA &&
          this->rx_message_[i+3] == 0x1C) {
        this->protocol_version_ = 3;
        ESP_LOGI(TAG, "Auto-detected v3 device from 0x1C heartbeats");
        return;
      }
    }
  }

  // Look for v0/v1 heartbeats (0x00) - indicates v0/v1 device
  if (this->rx_message_.size() >= 7) {
    for (size_t i = 0; i < this->rx_message_.size() - 6; i++) {
      if (this->rx_message_[i] == 0x55 &&
          this->rx_message_[i+1] == 0xAA &&
          this->rx_message_[i+3] == 0x00) {
        this->protocol_version_ = 0;
        ESP_LOGI(TAG, "Auto-detected v0/v1 device from 0x00 heartbeats");
        return;
      }
    }
  }
}

void Tuya::process_frames_() {
  int processed_frames = 0;
  const int MAX_FRAMES_PER_LOOP = 5;
  static uint32_t partial_frame_timeout = 0;

  // CONTROLLED LOGGING: Only log buffer size changes or when we have data
  static size_t last_logged_size = 0;
  static bool logged_no_data = false;

  if (this->rx_message_.empty()) {
    if (!logged_no_data) {
      ESP_LOGD("Tuya", "PROCESSING FRAMES - Buffer size: %zu (no new data)", this->rx_message_.size());
      logged_no_data = true;
    }
    return;  // Skip processing if no data
  }

  // Reset flag when we have data
  logged_no_data = false;

  // Only log buffer size changes
  if (this->rx_message_.size() != last_logged_size) {
    ESP_LOGD("Tuya", "PROCESSING FRAMES - Buffer size: %zu", this->rx_message_.size());
    last_logged_size = this->rx_message_.size();
  }

  while (processed_frames < MAX_FRAMES_PER_LOOP && this->rx_message_.size() >= 7) {
    // Smart header search instead of single-byte removal
    size_t header_pos = find_frame_header();
    if (header_pos != std::string::npos && header_pos > 0) {
      ESP_LOGD("Tuya", "Found header at position %zu, discarding %zu garbage bytes",
               header_pos, header_pos);
      this->rx_message_.erase(this->rx_message_.begin(),
                             this->rx_message_.begin() + header_pos);
      continue;  // Start over with clean buffer
    }

    if (this->rx_message_[0] != 0x55 || this->rx_message_[1] != 0xAA) {
      ESP_LOGD("Tuya", "No valid header found, clearing buffer");
      this->rx_message_.clear();
      partial_frame_timeout = 0;
      return;
    }

    uint8_t version = this->rx_message_[2];
    uint8_t command = this->rx_message_[3];
    uint16_t length = (uint16_t(this->rx_message_[4]) << 8) | this->rx_message_[5];
    size_t total_len = length + 7;

    if (length > 1024) {
      ESP_LOGW(TAG, "Excessive packet length: %u", length);
      this->rx_message_.clear();
      return;
    }

    // Timeout for partial frames
    if (this->rx_message_.size() < total_len) {
      if (partial_frame_timeout == 0) {
        partial_frame_timeout = millis();
        ESP_LOGD("Tuya", "Incomplete frame - need %zu bytes, have %zu", total_len, this->rx_message_.size());
      } else if (millis() - partial_frame_timeout > 1000) {  // 1 second timeout
        ESP_LOGW(TAG, "Partial frame timeout, clearing buffer");
        this->rx_message_.clear();
        partial_frame_timeout = 0;
      }
      break;  // Still waiting for data
    }
    partial_frame_timeout = 0;  // Reset timeout on complete frame

    // Validate checksum
    uint8_t checksum = 0;
    for (size_t i = 0; i < total_len - 1; i++) {
      checksum += this->rx_message_[i];
    }

    if (checksum != this->rx_message_[total_len - 1]) {
      ESP_LOGW(TAG, "Checksum Fail! Expected 0x%02X, got 0x%02X", checksum, this->rx_message_[total_len - 1]);
      ESP_LOGD(TAG, "Removing entire corrupted frame (%zu bytes)", total_len);
      this->rx_message_.erase(this->rx_message_.begin(),
                             this->rx_message_.begin() + total_len);
      continue;  // Start over
    }

    // Your excellent debugging - KEEP THIS (events only!)
    ESP_LOGD("Tuya", "=== COMPLETE FRAME DETECTED ===");
    ESP_LOGD("Tuya", "  Command: 0x%02X", command);
    ESP_LOGD("Tuya", "  Version: %u", version);
    ESP_LOGD("Tuya", "  Length: %u", length);
    ESP_LOGD("Tuya", "  Total Length: %zu", total_len);
    ESP_LOGD("Tuya", "  Checksum: 0x%02X", checksum);

    const uint8_t *payload_ptr = &this->rx_message_[6];
    this->handle_command_(command, version, payload_ptr, length);

    this->rx_message_.erase(this->rx_message_.begin(), this->rx_message_.begin() + total_len);
    processed_frames++;

    ESP_LOGD("Tuya", "Frame processed successfully, %u frames processed this loop", processed_frames);
  }

  // CONTROLLED LOGGING: Only log completion when we actually processed frames
  if (processed_frames > 0) {
    ESP_LOGD("Tuya", "Frame processing complete - processed %u frames, remaining buffer size: %zu",
             processed_frames, this->rx_message_.size());
  }
}

// Helper function to add (unchanged)
size_t Tuya::find_frame_header() {
  for (size_t i = 0; i < this->rx_message_.size() - 1; i++) {
    if (this->rx_message_[i] == 0x55 && this->rx_message_[i + 1] == 0xAA) {
      return i;
    }
  }
  return std::string::npos;
}

bool Tuya::validate_frame(const std::vector<uint8_t>& frame) {
  // Check minimum size
  if (frame.size() < 8) return false;

  // Validate header
  if (frame[0] != 0x55 || frame[1] != 0xAA) return false;

  // Validate checksum
  uint8_t checksum = 0;
  for (size_t i = 0; i < frame.size() - 1; i++) {
    checksum ^= frame[i];
  }
  if (checksum != frame.back()) {
    ESP_LOGW(TAG, "Invalid checksum: expected 0x%02X, got 0x%02X",
             checksum, frame.back());
    return false;
  }

  // Validate command
  uint8_t command = frame[3];
  if (!is_valid_tuya_command(command)) {
    ESP_LOGW(TAG, "Unknown command: 0x%02X", command);
    return false;
  }

  return true;
}

bool Tuya::is_valid_tuya_command(uint8_t command) {
  switch (command) {
    case TuyaCommandType::HEARTBEAT:
    case TuyaCommandType::PRODUCT_QUERY:
    case TuyaCommandType::CONF_QUERY:
    case TuyaCommandType::WIFI_STATE:
    case TuyaCommandType::DATAPOINT_DELIVER:  // Covers both 0x07 uses
    case TuyaCommandType::DATAPOINT_REPORT_SYNC:
    case TuyaCommandType::DATAPOINT_QUERY:
    case TuyaCommandType::DATAPOINT_REPORT_ACK:
    case TuyaCommandType::WIFI_TEST:
    case TuyaCommandType::WIFI_RSSI:
    case TuyaCommandType::LOCAL_TIME_QUERY:
    case TuyaCommandType::GET_NETWORK_STATUS:
    case TuyaCommandType::EXTENDED_SERVICES:
    case TuyaCommandType::VACUUM_MAP_UPLOAD:
      return true;
    default:
      return false;
  }
}

void Tuya::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya:");
  if (this->init_state_ != TuyaInitState::INIT_DONE) {
    if (this->init_failed_) {
      ESP_LOGCONFIG(TAG, "  Initialization failed. Current init_state: %u", static_cast<uint8_t>(this->init_state_));
    } else {
      ESP_LOGCONFIG(TAG, "  Configuration will be reported when setup is complete. Current init_state: %u",
                    static_cast<uint8_t>(this->init_state_));
    }
    ESP_LOGCONFIG(TAG, "  If no further output is received, confirm that this is a supported Tuya device.");
    return;
  }

  ESP_LOGCONFIG(TAG, "  Protocol version: %u", this->protocol_version_);
  ESP_LOGCONFIG(TAG, "  Product: '%s'", this->product_.c_str());

  if (!this->product_.empty() && this->product_ != R"({"p":"INVALID"})") {
    ESP_LOGCONFIG(TAG, "  Product JSON parsed successfully");
  }

  ESP_LOGCONFIG(TAG, "  Datapoints:");
  for (auto &info : this->datapoints_) {
    if (info.type == TuyaDatapointType::RAW) {
      ESP_LOGCONFIG(TAG, "    Datapoint %u: raw (value: %s)", info.id, format_hex_pretty(info.value_raw).c_str());
    } else if (info.type == TuyaDatapointType::BOOLEAN) {
      ESP_LOGCONFIG(TAG, "    Datapoint %u: switch (value: %s)", info.id, ONOFF(info.value_bool));
    } else if (info.type == TuyaDatapointType::INTEGER) {
      ESP_LOGCONFIG(TAG, "    Datapoint %u: int value (value: %d)", info.id, info.value_int);
    } else if (info.type == TuyaDatapointType::STRING) {
      ESP_LOGCONFIG(TAG, "    Datapoint %u: string value (value: %s)", info.id, info.value_string.c_str());
    } else if (info.type == TuyaDatapointType::ENUM) {
      ESP_LOGCONFIG(TAG, "    Datapoint %u: enum (value: %d)", info.id, info.value_enum);
    } else if (info.type == TuyaDatapointType::BITMASK) {
      ESP_LOGCONFIG(TAG, "    Datapoint %u: bitmask (value: %" PRIx32 ")", info.id, info.value_bitmask);
    } else {
      ESP_LOGCONFIG(TAG, "    Datapoint %u: unknown", info.id);
    }
  }

  if ((this->status_pin_reported_ != -1) || (this->reset_pin_reported_ != -1)) {
    ESP_LOGCONFIG(TAG, "  GPIO Configuration: status: pin %d, reset: pin %d", this->status_pin_reported_,
                  this->reset_pin_reported_);
  }

  LOG_PIN("  Status Pin: ", this->status_pin_);

  // Enhanced device type detection info
  ESP_LOGCONFIG(TAG, "  Initialization Type: %u", static_cast<uint8_t>(this->init_type_));

  switch (this->init_type_) {
    case TuyaInitType::FULL_HANDSHAKE:
      ESP_LOGCONFIG(TAG, "  Device Behavior: Full handshake (standard v0/v1)");
      ESP_LOGCONFIG(TAG, "  Heartbeat Interval: 15s");
      break;
    case TuyaInitType::PARTIAL_HANDSHAKE:
      ESP_LOGCONFIG(TAG, "  Device Behavior: Partial handshake (problematic)");
      ESP_LOGCONFIG(TAG, "  Heartbeat Interval: 30s");
      break;
    case TuyaInitType::HEARTBEAT_ONLY:
      ESP_LOGCONFIG(TAG, "  Device Behavior: Heartbeat-only (no handshake)");
      ESP_LOGCONFIG(TAG, "  Heartbeat Interval: 15s");
      break;
  }

  // Protocol-specific device type info
  if (this->protocol_version_ == 0) {
    ESP_LOGCONFIG(TAG, "  Protocol Type: Legacy v0 (requires periodic heartbeats)");
  } else if (this->protocol_version_ == 3) {
    ESP_LOGCONFIG(TAG, "  Protocol Type: Modern v3 (heartbeat-free after init)");
  } else {
    ESP_LOGCONFIG(TAG, "  Protocol Type: Unknown protocol version %u", this->protocol_version_);
  }
}

void Tuya::process_init_step_() {
  static uint32_t last_init_time = 0;
  uint32_t now = millis();

  // Only process one init step every 20ms to avoid blocking
  if (now - last_init_time < 20) return;
  last_init_time = now;

  switch (this->init_state_) {
    case TuyaInitState::INIT_HEARTBEAT:
      if (this->rx_message_.empty()) {
        this->send_empty_command_(TuyaCommandType::HEARTBEAT);
        this->init_state_ = TuyaInitState::INIT_PRODUCT;
      }
      break;
    case TuyaInitState::INIT_PRODUCT:
      if (this->product_ != "") {  // Product received
        this->send_empty_command_(TuyaCommandType::CONF_QUERY);
        this->init_state_ = TuyaInitState::INIT_CONF;
      }
      break;
    case TuyaInitState::INIT_CONF:
      if (this->status_pin_reported_ != -1) {  // Config received
        this->init_state_ = TuyaInitState::INIT_DONE;
        ESP_LOGI(TAG, "Init complete - switching to event-driven mode");
      }
      break;
    default:
      break;
  }
}

// In handle_command_(), move the protocol detection to the VERY START
void Tuya::handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
  // Add protocol detection
  if (!this->protocol_detected_) {
    if (command == 0x1C && version != 0xFF) {
      this->protocol_version_ = 3;
      this->protocol_detected_ = true;
      ESP_LOGI(TAG, "Auto-detected protocol version: 3 (v3 device)");
    } else if (command == 0x00 && version != 0xFF) {
      this->protocol_version_ = 0;
      this->protocol_detected_ = true;
      ESP_LOGI(TAG, "Auto-detected protocol version: 0 (v0/v1 device)");
    }
  }

  // ADD THIS BLOCK AT THE VERY START - Detect protocol version immediately
  if (version != 0xFF && this->protocol_version_ == 0xFF) {
    this->protocol_version_ = version;
    ESP_LOGI(TAG, "Detected protocol version: %u", version);
  }

  // ADD THIS BLOCK IMMEDIATELY AFTER - Detect protocol from heartbeat command
  if (command == 0x1C && this->protocol_version_ != 3) {
    this->protocol_version_ = 3;
    ESP_LOGI(TAG, "Forced protocol version to 3 (detected v3 heartbeat command)");
  } else if (command == 0x00 && this->protocol_version_ == 0xFF) {
    this->protocol_version_ = 0;
    ESP_LOGI(TAG, "Detected protocol version: 0 (v0/v1 heartbeat)");
  }

  // Track heartbeat timing for device type detection
  if (command == 0x00 || command == 0x1C) {
    this->last_heartbeat_time_ = millis();
  }

  // ADD THIS BLOCK - Debug what we received with dual-purpose awareness
  ESP_LOGD(TAG, "=== RECEIVED COMMAND ===");
  ESP_LOGD(TAG, "  Command: 0x%02X (%s)", command, command_type_to_string((TuyaCommandType)command).c_str());
  ESP_LOGD(TAG, "  Version: %u", version);
  ESP_LOGD(TAG, "  Len: %zu", len);
  ESP_LOGD(TAG, "  Dual-purpose 0x07: %s", (command == 0x07) ? "YES" : "NO");
  ESP_LOGD(TAG, "  Init State: %u", static_cast<uint8_t>(this->init_state_));
  ESP_LOGD(TAG, "  Protocol Version: %u", this->protocol_version_);
  ESP_LOGD(TAG, "======================");

  ESP_LOGD(TAG, "HANDLE COMMAND: cmd=0x%02X, version=%u, len=%zu, init_state=%u, expected_response=%s",
           command, version, len, static_cast<uint8_t>(this->init_state_),
           this->expected_response_.has_value() ?
             std::to_string((uint8_t)*this->expected_response_).c_str() : "NONE");

  // Only log expected response changes (not every heartbeat)
  if (this->expected_response_.has_value()) {
    if (this->expected_response_.value() != this->last_expected_response_) {
      ESP_LOGD(TAG, "Expected response changed: 0x%02X", (uint8_t)*this->expected_response_);
      this->last_expected_response_ = this->expected_response_.value();
    }
  }

  // Handle heartbeats (both v0/v1 0x00 and v3 0x1C)
  if (command == 0x00 || command == 0x1C) {
    ESP_LOGD(TAG, "=== HEARTBEAT DETECTED ===");
    ESP_LOGD(TAG, "  Command: 0x%02X", command);
    ESP_LOGD(TAG, "  Protocol Version: %u", this->protocol_version_);
    ESP_LOGD(TAG, "  Init State: %u", static_cast<uint8_t>(this->init_state_));

    if (command == 0x00) {
      ESP_LOGD(TAG, "  Type: V0/V1 Heartbeat");
    } else if (command == 0x1C) {
      ESP_LOGD(TAG, "  Type: V3 Heartbeat");
    }
    ESP_LOGD(TAG, "========================");

    // Clear heartbeat timeout regardless of state
    if (this->expected_response_.has_value() && *this->expected_response_ == TuyaCommandType::HEARTBEAT) {
      ESP_LOGD(TAG, "HEARTBEAT RESPONSE MATCHED - clearing expected response");
      this->expected_response_.reset();
      this->last_command_timestamp_ = 0;
      this->init_retries_ = 0;
    }

    // Always respond to heartbeat
    this->send_empty_command_(TuyaCommandType::HEARTBEAT);

    // Progress initialization
    if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT) {
      this->init_state_ = TuyaInitState::INIT_PRODUCT;
      this->send_empty_command_(TuyaCommandType::PRODUCT_QUERY);
    }
  } else {
    // Check if this response matches what we expected (only log mismatches)
    if (this->expected_response_.has_value() && command == *this->expected_response_) {
      ESP_LOGD(TAG, "RESPONSE MATCHES EXPECTED: 0x%02X", command);
      this->expected_response_.reset();
      this->last_command_timestamp_ = 0;
      this->init_retries_ = 0;
    } else if (this->expected_response_.has_value() && command != *this->expected_response_) {
      ESP_LOGD(TAG, "RESPONSE DOESN'T MATCH EXPECTED: expected=0x%02X, got=0x%02X",
               (uint8_t)*this->expected_response_, command);
    }
  }

  // Handle dual-purpose 0x07 command (DATAPOINT_DELIVER/DATAPOINT_REPORT_ASYNC)
  bool is_dual_purpose_07 = (command == 0x07);

  // Rest of the switch statement...
  switch (command) {
    case TuyaCommandType::PRODUCT_QUERY: {
      // ADD THIS BLOCK - Debug raw response
      ESP_LOGD(TAG, "=== PRODUCT QUERY RESPONSE ===");
      ESP_LOGD(TAG, "  Raw data: %s", format_hex_pretty(buffer, len).c_str());
      ESP_LOGD(TAG, "  As string: '%.*s'", (int)len, buffer);

      // Check for common JSON patterns
      bool starts_with_brace = (len > 0 && buffer[0] == '{');
      bool ends_with_brace = (len > 0 && buffer[len-1] == '}');
      bool has_quotes = (len > 0 && std::find(buffer, buffer + len, '"') != buffer + len);

      ESP_LOGD(TAG, "  JSON indicators: starts_with_brace=%s, ends_with_brace=%s, has_quotes=%s",
              starts_with_brace ? "YES" : "NO",
              ends_with_brace ? "YES" : "NO",
              has_quotes ? "YES" : "NO");

      // Try multiple parsing approaches
      std::string product_data(reinterpret_cast<const char *>(buffer), len);

      // Method 1: JSON parsing
      bool is_json = (len >= 2 && buffer[0] == '{' && buffer[len-1] == '}');
      if (is_json) {
        this->product_ = product_data;
        ESP_LOGD(TAG, "JSON product data: %s", product_data.c_str());
      }
      // Method 2: Plain text validation
      else {
        bool valid = true;
        for (size_t i = 0; i < len; i++) {
          if (!std::isprint(buffer[i])) {
            valid = false;
            break;
          }
        }
        if (valid && len > 0) {
          this->product_ = product_data;
          ESP_LOGD(TAG, "Plain product data: %s", product_data.c_str());
        }
        // Method 3: Fallback if parsing fails
        else {
          this->product_ = "unknown_v3_device";
          ESP_LOGW(TAG, "Product parsing failed, using fallback");
        }
      }

      ESP_LOGD(TAG, "  Final product value: '%s'", this->product_.c_str());
      ESP_LOGD(TAG, "=============================");

      if (this->init_state_ == TuyaInitState::INIT_PRODUCT) {
        this->init_state_ = TuyaInitState::INIT_CONF;
        this->send_empty_command_(TuyaCommandType::CONF_QUERY);
      }
      break;
    }


    case TuyaCommandType::CONF_QUERY: {
      // Parse status and reset pins if provided
      if (len >= 2) {
        this->status_pin_reported_ = buffer[0];
        this->reset_pin_reported_ = buffer[1];
      }

      // Always send WiFi status in response for v3+ devices
      if (this->protocol_version_ >= 3) {
        this->send_command_(TuyaCommand{TuyaCommandType::GET_NETWORK_STATUS, {this->get_wifi_status_code_()}});
      } else {
        // For v0/v1 devices, move directly to done
        this->init_state_ = TuyaInitState::INIT_DONE;
        this->initialized_callback_.call();
        this->dump_config();
      }
      break;
    }

    case TuyaCommandType::GET_NETWORK_STATUS: {
      // Always respond with current status
      this->send_command_(TuyaCommand{TuyaCommandType::GET_NETWORK_STATUS, {this->get_wifi_status_code_()}});

      // Complete initialization for v3+ devices
      if (this->init_state_ == TuyaInitState::INIT_CONF) {
        this->init_state_ = TuyaInitState::INIT_DONE;
        this->initialized_callback_.call();
        this->dump_config();
      }
      break;
    }

    case TuyaCommandType::DATAPOINT_DELIVER: {
      // Handle dual-purpose 0x07 command
      ESP_LOGD(TAG, "Processing dual-purpose 0x07 command: %s",
               is_dual_purpose_07 ? "DATAPOINT_DELIVER/REPORT_ASYNC" : "DATAPOINT_DELIVER");

      // Handle datapoints and complete initialization
      this->handle_datapoints_(buffer, len);

      // For DATAPOINT_DELIVER, send ACK if needed
      if (this->expected_response_.has_value() && *this->expected_response_ == TuyaCommandType::DATAPOINT_DELIVER) {
        this->send_command_(
            TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_REPORT_ACK, .payload = std::vector<uint8_t>{0x01}});
        this->expected_response_.reset();
      }

      // Complete initialization if needed
      if (this->init_state_ != TuyaInitState::INIT_DONE) {
        this->init_state_ = TuyaInitState::INIT_DONE;
        this->initialized_callback_.call();
        this->dump_config();
      }
      break;
    }

    case TuyaCommandType::DATAPOINT_REPORT_SYNC: {
      // Handle sync reports separately
      ESP_LOGD(TAG, "Processing DATAPOINT_REPORT_SYNC");
      this->handle_datapoints_(buffer, len);
      this->send_command_(
          TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_REPORT_ACK, .payload = std::vector<uint8_t>{0x01}});
      break;
    }

    case TuyaCommandType::DATAPOINT_QUERY: {
      ESP_LOGD(TAG, "Processing DATAPOINT_QUERY");
      // Handle queries - typically just respond with current datapoint values
      this->send_command_(TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_DELIVER, .payload = std::vector<uint8_t>{}});
      break;
    }

    default: {
      // Handle unknown commands gracefully
      if (command == 0x07) {
        // This is the dual-purpose 0x07 command that we might not have handled yet
        ESP_LOGD(TAG, "Received unexpected 0x07 command (dual-purpose), handling as datapoint");
        this->handle_datapoints_(buffer, len);
      } else {
        ESP_LOGD(TAG, "Unknown command: 0x%02X", command);
      }
      break;
    }
  }
}

void Tuya::handle_datapoints_(const uint8_t *buffer, size_t len) {
  int processed_datapoints = 0;
  const int MAX_DATPOINTS_PER_CALL = 10;

  while (len >= 4 && processed_datapoints < MAX_DATPOINTS_PER_CALL) {
    TuyaDatapoint datapoint{};
    datapoint.id = buffer[0];
    datapoint.type = (TuyaDatapointType) buffer[1];
    datapoint.value_uint = 0;

    size_t data_size = (uint16_t(buffer[2]) << 8) | buffer[3];
    const uint8_t *data = buffer + 4;

    if (data_size > 1024) {
      ESP_LOGW(TAG, "Datapoint %u has excessive data size: %zu", datapoint.id, data_size);
      return;
    }

    if (data_size + 4 > len) {
      ESP_LOGW(TAG, "Datapoint %u truncated! Expected %zu, got %zu", datapoint.id, data_size + 4, len);
      return;
    }
    datapoint.len = data_size;

    bool skip = false;
    for (uint8_t ignored_id : this->ignore_mcu_update_on_datapoints_) {
      if (datapoint.id == ignored_id) { skip = true; break; }
    }

    switch (datapoint.type) {
      case TuyaDatapointType::RAW:
        datapoint.value_raw.assign(data, data + data_size);
        break;

      case TuyaDatapointType::BOOLEAN:
        if (data_size != 1) { ESP_LOGW(TAG, "DP %u bad bool len", datapoint.id); return; }
        datapoint.value_bool = data[0];
        if (!skip) ESP_LOGD(TAG, "DP %u (Bool): %s", datapoint.id, ONOFF(datapoint.value_bool));
        break;

      case TuyaDatapointType::INTEGER:
        if (data_size != 4) { ESP_LOGW(TAG, "DP %u bad int len", datapoint.id); return; }
        datapoint.value_uint = encode_uint32(data[0], data[1], data[2], data[3]);
        if (!skip) ESP_LOGD(TAG, "DP %u (Int): %u", datapoint.id, datapoint.value_uint);
        break;

      case TuyaDatapointType::STRING:
        datapoint.value_string = std::string(reinterpret_cast<const char *>(data), data_size);
        if (!skip) ESP_LOGD(TAG, "DP %u (String): %s", datapoint.id, datapoint.value_string.c_str());
        break;

      case TuyaDatapointType::ENUM:
        if (data_size != 1) { ESP_LOGW(TAG, "DP %u bad enum len", datapoint.id); return; }
        datapoint.value_enum = data[0];
        if (!skip) ESP_LOGD(TAG, "DP %u (Enum): %u", datapoint.id, datapoint.value_enum);
        break;

      case TuyaDatapointType::BITMASK:
        if (data_size == 1) datapoint.value_bitmask = encode_uint32(0, 0, 0, data[0]);
        else if (data_size == 2) datapoint.value_bitmask = encode_uint32(0, 0, data[0], data[1]);
        else if (data_size == 4) datapoint.value_bitmask = encode_uint32(data[0], data[1], data[2], data[3]);
        else { ESP_LOGW(TAG, "DP %u bad bitmask len", datapoint.id); return; }
        if (!skip) ESP_LOGD(TAG, "DP %u (Bitmask): %08X", datapoint.id, datapoint.value_bitmask);
        break;

      default:
        ESP_LOGW(TAG, "DP %u unknown type 0x%02X", datapoint.id, (uint8_t)datapoint.type);
        return;
    }

    if (!skip) {
      auto it = std::find_if(this->datapoints_.begin(), this->datapoints_.end(),
                             [&](const TuyaDatapoint &d) { return d.id == datapoint.id; });
      if (it != this->datapoints_.end()) {
        *it = datapoint;
      } else {
        this->datapoints_.push_back(datapoint);
      }

      for (auto &listener : this->listeners_) {
        if (listener.datapoint_id == datapoint.id) {
          listener.on_datapoint(datapoint);
        }
      }
    }

    len -= (data_size + 4);
    buffer += (data_size + 4);
    processed_datapoints++;
  }
}

// void Tuya::send_raw_command_(TuyaCommand command) {
//   if (command.payload.size() > 1024) {
//     ESP_LOGE(TAG, "Command payload too large: %zu bytes", command.payload.size());
//     return;
//   }

//   uint8_t len_hi = (uint8_t) (command.payload.size() >> 8);
//   uint8_t len_lo = (uint8_t) (command.payload.size() & 0xFF);
//   uint8_t version = (this->protocol_version_ == 0xFF) ? 0x03 : this->protocol_version_;  // Default to v3

// // void Tuya::send_raw_command_(TuyaCommand command) {
// //   ESP_LOGD(TAG, "SENDING COMMAND: cmd=0x%02X, payload_size=%zu, protocol_version=%u",
// //            static_cast<uint8_t>(command.cmd), command.payload.size(), this->protocol_version_);

// //   if (command.payload.size() > 1024) {
// //     ESP_LOGE(TAG, "Command payload too large: %zu bytes", command.payload.size());
// //     return;
// //   }

// //   uint8_t version = (this->protocol_version_ == 0xFF) ? 0x00 : this->protocol_version_;
// //   ESP_LOGD(TAG, "Using version: %u (detected=%u)", version, this->protocol_version_);

//   this->last_command_timestamp_ = millis();

//   switch (command.cmd) {
//     case TuyaCommandType::HEARTBEAT: this->expected_response_ = TuyaCommandType::HEARTBEAT; break;
//     case TuyaCommandType::PRODUCT_QUERY: this->expected_response_ = TuyaCommandType::PRODUCT_QUERY; break;
//     case TuyaCommandType::CONF_QUERY: this->expected_response_ = TuyaCommandType::CONF_QUERY; break;
//     case TuyaCommandType::DATAPOINT_DELIVER:
//     case TuyaCommandType::DATAPOINT_QUERY: this->expected_response_ = TuyaCommandType::DATAPOINT_REPORT_ASYNC; break;
//     default: break;
//   }

//   ESP_LOGV(TAG, "TX: CMD=0x%02X VER=%u LEN=%d", static_cast<uint8_t>(command.cmd), version, command.payload.size());

//   this->write_array({0x55, 0xAA, version, (uint8_t) command.cmd, len_hi, len_lo});
//   if (!command.payload.empty()) {
//     this->write_array(command.payload.data(), command.payload.size());
//   }

//   uint8_t checksum = 0x55 + 0xAA + version + (uint8_t) command.cmd + len_hi + len_lo;
//   for (auto &data : command.payload) {
//     checksum += data;
//   }
//   this->write_byte(checksum);
// }

// void Tuya::send_raw_command_(TuyaCommand command) {
//   // ADD THIS BLOCK - Debug what we're sending
//   ESP_LOGD(TAG, "=== SENDING COMMAND ===");
//   ESP_LOGD(TAG, "  Command: 0x%02X (%s)", (uint8_t)command.cmd,
//            command_type_to_string(command.cmd).c_str());

//   // CORRECT THE PROTOCOL VERSION LOGIC
//   uint8_t version;
//   if (this->protocol_version_override_ != 0xFF) {
//     version = this->protocol_version_override_;  // Use override version
//     ESP_LOGD(TAG, "  Protocol Version: Override forced to %u", version);
//   } else if (this->protocol_version_ != 0xFF) {
//     version = this->protocol_version_;  // Use detected version
//     ESP_LOGD(TAG, "  Protocol Version: Auto-detected %u", version);
//   } else {
//     version = 0x00;  // Default to v0
//     ESP_LOGD(TAG, "  Protocol Version: Defaulting to 0");
//   }

//   ESP_LOGD(TAG, "  Using Version: %u", version);
//   ESP_LOGD(TAG, "  Payload Size: %zu", command.payload.size());
//   ESP_LOGD(TAG, "  Dual-purpose 0x07: %s", (command.cmd == TuyaCommandType::DATAPOINT_DELIVER) ? "YES" : "NO");
//   ESP_LOGD(TAG, "======================");

//   // DECLARE cmd_byte ONCE HERE
//   uint8_t cmd_byte = (uint8_t) command.cmd;

//   // Handle protocol-specific command mapping
//   if (command.cmd == TuyaCommandType::HEARTBEAT) {
//     if (version == 3) {
//       cmd_byte = 0x1C;  // v3 heartbeat
//       ESP_LOGD(TAG, "SENDING V3 HEARTBEAT: 0x%02X", cmd_byte);
//     } else {
//       cmd_byte = 0x00;  // v0/v1 heartbeat
//       ESP_LOGD(TAG, "SENDING V0/V1 HEARTBEAT: 0x%02X", cmd_byte);
//     }
//   }

//   ESP_LOGD(TAG, "  Final Cmd Byte: 0x%02X", cmd_byte);

//   uint8_t len_hi = (uint8_t) (command.payload.size() >> 8);
//   uint8_t len_lo = (uint8_t) (command.payload.size() & 0xFF);

//   this->last_command_timestamp_ = millis();

//   // Set expected response with dual-purpose awareness
//   switch (command.cmd) {
//     case TuyaCommandType::HEARTBEAT:
//       this->expected_response_ = TuyaCommandType::HEARTBEAT;
//       break;
//     case TuyaCommandType::PRODUCT_QUERY:
//       this->expected_response_ = TuyaCommandType::PRODUCT_QUERY;
//       break;
//     case TuyaCommandType::CONF_QUERY:
//       this->expected_response_ = TuyaCommandType::CONF_QUERY;
//       break;
//     case TuyaCommandType::DATAPOINT_DELIVER:
//     case TuyaCommandType::DATAPOINT_QUERY:
//       // For 0x07, expect either DATAPOINT_DELIVER or DATAPOINT_REPORT_ASYNC response
//       this->expected_response_ = TuyaCommandType::DATAPOINT_DELIVER;
//       break;
//     case TuyaCommandType::GET_NETWORK_STATUS:
//       this->expected_response_ = TuyaCommandType::GET_NETWORK_STATUS;
//       break;
//     default:
//       break;
//   }

//   ESP_LOGV(TAG, "Sending Tuya: CMD=0x%02X VERSION=%u DATA=[%s] INIT_STATE=%u",
//            cmd_byte, version, format_hex_pretty(command.payload).c_str(),
//            static_cast<uint8_t>(this->init_state_));

//   // ADD THIS BLOCK - Log raw bytes being sent
//   ESP_LOGD(TAG, "RAW TX: 55:AA:%02X:%02X:%02X:%02X",
//            version, cmd_byte, len_hi, len_lo);

//   this->write_array({0x55, 0xAA, version, cmd_byte, len_hi, len_lo});

//   // ADD THIS BLOCK - Log payload if present
//   if (!command.payload.empty()) {
//     ESP_LOGD(TAG, "RAW TX PAYLOAD: %s", format_hex_pretty(command.payload).c_str());
//     this->write_array(command.payload.data(), command.payload.size());
//   }

//   uint8_t checksum = 0x55 + 0xAA + cmd_byte + len_hi + len_lo;
//   for (auto &data : command.payload)
//     checksum += data;
//   this->write_byte(checksum);

//   // ADD THIS BLOCK - Log final checksum
//   ESP_LOGD(TAG, "RAW TX CHECKSUM: %02X", checksum);
// }

void Tuya::send_raw_command_(TuyaCommand command) {
  // ADD THIS BLOCK - Debug what we're sending
  ESP_LOGD(TAG, "=== SENDING COMMAND ===");
  ESP_LOGD(TAG, "  Command: 0x%02X (%s)", (uint8_t)command.cmd,
           command_type_to_string(command.cmd).c_str());

  // CORRECT THE PROTOCOL VERSION LOGIC
  uint8_t version;
  if (this->protocol_version_override_ != 0xFF) {
    version = this->protocol_version_override_;  // Use override version
    ESP_LOGD(TAG, "  Protocol Version: Override forced to %u", version);
  } else if (this->protocol_version_ != 0xFF) {
    version = this->protocol_version_;  // Use detected version
    ESP_LOGD(TAG, "  Protocol Version: Auto-detected %u", version);
  } else {
    version = 0x00;  // Default to v0
    ESP_LOGD(TAG, "  Protocol Version: Defaulting to 0");
  }

  ESP_LOGD(TAG, "  Using Version: %u", version);
  ESP_LOGD(TAG, "  Payload Size: %zu", command.payload.size());
  ESP_LOGD(TAG, "  Dual-purpose 0x07: %s", (command.cmd == TuyaCommandType::DATAPOINT_DELIVER) ? "YES" : "NO");
  ESP_LOGD(TAG, "======================");

  // DECLARE cmd_byte ONCE HERE
  uint8_t cmd_byte = (uint8_t) command.cmd;

  // Handle protocol-specific command mapping
  if (command.cmd == TuyaCommandType::HEARTBEAT) {
    if (version == 3) {
      cmd_byte = 0x1C;  // v3 heartbeat
      ESP_LOGD(TAG, "SENDING V3 HEARTBEAT: 0x%02X", cmd_byte);
    } else {
      cmd_byte = 0x00;  // v0/v1 heartbeat
      ESP_LOGD(TAG, "SENDING V0/V1 HEARTBEAT: 0x%02X", cmd_byte);
    }
  }

  ESP_LOGD(TAG, "  Final Cmd Byte: 0x%02X", cmd_byte);

  uint8_t len_hi = (uint8_t) (command.payload.size() >> 8);
  uint8_t len_lo = (uint8_t) (command.payload.size() & 0xFF);

  this->last_command_timestamp_ = millis();

  // Set expected response with dual-purpose awareness
  switch (command.cmd) {
    case TuyaCommandType::HEARTBEAT:
      this->expected_response_ = TuyaCommandType::HEARTBEAT;
      break;
    case TuyaCommandType::PRODUCT_QUERY:
      this->expected_response_ = TuyaCommandType::PRODUCT_QUERY;
      break;
    case TuyaCommandType::CONF_QUERY:
      this->expected_response_ = TuyaCommandType::CONF_QUERY;
      break;
    case TuyaCommandType::DATAPOINT_DELIVER:
    case TuyaCommandType::DATAPOINT_QUERY:
      // For 0x07, expect either DATAPOINT_DELIVER or DATAPOINT_REPORT_ASYNC response
      this->expected_response_ = TuyaCommandType::DATAPOINT_DELIVER;
      break;
    case TuyaCommandType::GET_NETWORK_STATUS:
      this->expected_response_ = TuyaCommandType::GET_NETWORK_STATUS;
      break;
    default:
      break;
  }

  ESP_LOGV(TAG, "Sending Tuya: CMD=0x%02X VERSION=%u DATA=[%s] INIT_STATE=%u",
           cmd_byte, version, format_hex_pretty(command.payload).c_str(),
           static_cast<uint8_t>(this->init_state_));

  // ADD THIS BLOCK - Log raw bytes being sent
  ESP_LOGD(TAG, "RAW TX: 55:AA:%02X:%02X:%02X:%02X",
           version, cmd_byte, len_hi, len_lo);

  this->write_array({0x55, 0xAA, version, cmd_byte, len_hi, len_lo});

  // ADD THIS BLOCK - Log payload if present
  if (!command.payload.empty()) {
    ESP_LOGD(TAG, "RAW TX PAYLOAD: %s", format_hex_pretty(command.payload).c_str());
    this->write_array(command.payload.data(), command.payload.size());
  }

  uint8_t checksum = 0x55 + 0xAA + cmd_byte + len_hi + len_lo;
  for (auto &data : command.payload)
    checksum += data;
  this->write_byte(checksum);

  // ADD THIS BLOCK - Log final checksum
  ESP_LOGD(TAG, "RAW TX CHECKSUM: %02X", checksum);
}

// void Tuya::process_command_queue_() {
//   uint32_t now = millis();
//   static uint32_t last_process_time = 0;

//   // Only process every 5ms to avoid CPU churn
//   if (now - last_process_time < 5) {
//     return;
//   }
//   last_process_time = now;

//   // Always process incoming frames (this is essential)
//   this->process_frames_();

//   // Only handle command queue and timeouts during initialization
//   if (this->init_state_ != TuyaInitState::INIT_DONE) {
//     // Handle timeouts for initialization commands
//     if (this->expected_response_.has_value()) {
//       if (now - this->last_command_timestamp_ > 2000) {
//         ESP_LOGD(TAG, "Init timeout for 0x%02X, continuing", (uint8_t)*this->expected_response_);

//         if (*this->expected_response_ == TuyaCommandType::HEARTBEAT) {
//           ESP_LOGD(TAG, "Heartbeat timeout - clearing expected response and continuing");
//           this->expected_response_.reset();
//           this->last_command_timestamp_ = 0;
//           return;  // Don't retry, just continue
//         }

//         this->expected_response_.reset();

//         if (this->init_state_ != TuyaInitState::INIT_DONE) {
//           this->init_retries_++;
//           if (this->init_retries_ >= MAX_RETRIES) {
//             this->init_failed_ = true;
//             ESP_LOGE(TAG, "Initialization failed after %d retries", MAX_RETRIES);
//           }
//         }
//       }
//     }

//     // Send queued commands during init
//     if (!this->command_queue_.empty()) {
//       if (now - this->last_command_timestamp_ > COMMAND_DELAY) {
//         this->send_raw_command_(this->command_queue_.front());
//         this->command_queue_.pop_front();
//       }
//     }

//     // Send heartbeats only during INIT_HEARTBEAT state
//     if (this->init_state_ == TuyaInitState::INIT_HEARTBEAT && this->command_queue_.empty()) {
//       if (this->expected_response_ != TuyaCommandType::HEARTBEAT) {
//         // FIX: Use 0x1C for heartbeat instead of 0x00 for v3 devices
//         this->send_raw_command_(TuyaCommand{
//           TuyaCommandType::HEARTBEAT,
//           std::vector<uint8_t>()
//         });
//       }
//     }
//   }
//   // After INIT_DONE: DO NOTHING - just process frames silently
// }

void Tuya::process_command_queue_() {
  uint32_t now = millis();
  uint32_t delay = now - this->last_command_timestamp_;

  // Handle timeouts
  if (this->expected_response_.has_value() && delay > RECEIVE_TIMEOUT) {
    this->expected_response_.reset();
    if (this->init_state_ != TuyaInitState::INIT_DONE) {
      this->init_retries_++;
      if (this->init_retries_ >= MAX_RETRIES) {
        this->init_failed_ = true;
        ESP_LOGE(TAG, "Initialization failed at init_state %u", static_cast<uint8_t>(this->init_state_));
      }
    }
  }

  // Send commands
  if (delay > COMMAND_DELAY && !this->command_queue_.empty() && this->rx_message_.empty()) {
    this->send_raw_command_(command_queue_.front());
    if (!this->expected_response_.has_value())
      this->command_queue_.erase(command_queue_.begin());
  }
}

void Tuya::send_command_(const TuyaCommand &command) {
  if (command.payload.size() > 1024) {
    ESP_LOGE(TAG, "Command too large to queue: %zu bytes", command.payload.size());
    return;
  }
  command_queue_.push_back(command);
  process_command_queue_();
}

void Tuya::send_empty_command_(TuyaCommandType command) {
  send_command_(TuyaCommand{.cmd = command, .payload = std::vector<uint8_t>{}});
}

uint8_t Tuya::get_wifi_status_code_() {
  uint8_t status = 0x02;
  if (network::is_connected()) {
    status = 0x03;
    if (this->protocol_version_ >= 0x03 && remote_is_connected()) {
      status = 0x04;
    }
  } else {
#ifdef USE_CAPTIVE_PORTAL
    if (captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active()) {
      status = 0x01;
    }
#endif
  };
  return status;
}

#ifdef USE_TIME
void Tuya::send_local_time_() {
  std::vector<uint8_t> payload;
  ESPTime now = this->time_id_->now();
  if (now.is_valid()) {
    uint8_t year = now.year - 2000;
    uint8_t month = now.month;
    uint8_t day_of_month = now.day_of_month;
    uint8_t hour = now.hour;
    uint8_t minute = now.minute;
    uint8_t second = now.second;
    uint8_t day_of_week = now.day_of_week - 1;
    if (day_of_week == 0) {
      day_of_week = 7;
    }
    ESP_LOGD(TAG, "Sending local time");
    payload = std::vector<uint8_t>{0x01, year, month, day_of_month, hour, minute, second, day_of_week};
  } else {
    ESP_LOGW(TAG, "Sending missing local time");
    payload = std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::LOCAL_TIME_QUERY, .payload = payload});
}
#endif

optional<TuyaDatapoint> Tuya::get_datapoint_(uint8_t datapoint_id) {
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      return datapoint;
  }
  return nullopt;  // Change from return {} to nullopt
}

optional<TuyaDatapoint> Tuya::get_datapoint_value(uint8_t datapoint_id) {
  return this->get_datapoint_(datapoint_id);
}

void Tuya::set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint32_t value,
                                        uint8_t length, bool forced) {
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
  } else if (datapoint->type != datapoint_type) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_uint == value) {
    return;
  }

  std::vector<uint8_t> data;
  switch (length) {
    case 4:
      data.push_back(value >> 24);
      data.push_back(value >> 16);
    case 2:
      data.push_back(value >> 8);
    case 1:
      data.push_back(value >> 0);
      break;
    default:
      ESP_LOGE(TAG, "Unexpected datapoint length %u", length);
      return;
  }
  this->send_datapoint_command_(datapoint_id, datapoint_type, data);
}

void Tuya::set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced) {
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
  } else if (datapoint->type != TuyaDatapointType::RAW) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_raw == value) {
    return;
  }
  this->send_datapoint_command_(datapoint_id, TuyaDatapointType::RAW, value);
}

void Tuya::set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced) {
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
  } else if (datapoint->type != TuyaDatapointType::STRING) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_string == value) {
    return;
  }
  std::vector<uint8_t> data(value.begin(), value.end());
  this->send_datapoint_command_(datapoint_id, TuyaDatapointType::STRING, data);
}

// Public API methods (for child components)
void Tuya::set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::INTEGER, value, 4, false);
}

void Tuya::set_boolean_datapoint_value(uint8_t datapoint_id, bool value) {
  std::vector<uint8_t> data = {static_cast<uint8_t>(value ? 1 : 0)};
  this->set_raw_datapoint_value_(datapoint_id, data, false);
}

void Tuya::set_string_datapoint_value(uint8_t datapoint_id, const std::string &value) {
  this->set_string_datapoint_value_(datapoint_id, value, false);
}

void Tuya::force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::ENUM, value, 1, true);
}

void Tuya::force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::INTEGER, value, 4, true);
}

/// Set enum datapoint value (for select components)
void Tuya::set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::ENUM, value, 1, false);
}

/// Set raw datapoint value (for binary_sensor, climate raw data)
void Tuya::set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value) {
  this->set_raw_datapoint_value_(datapoint_id, value, false);
}

void Tuya::send_datapoint_command_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> buffer;
  buffer.push_back(datapoint_id);
  buffer.push_back(static_cast<uint8_t>(datapoint_type));
  buffer.push_back(data.size() >> 8);
  buffer.push_back(data.size() >> 0);
  buffer.insert(buffer.end(), data.begin(), data.end());

  this->send_command_(TuyaCommand{.cmd = TuyaCommandType::DATAPOINT_DELIVER, .payload = buffer});
}

void Tuya::register_listener(uint8_t datapoint_id, const std::function<void(TuyaDatapoint)> &func) {
  auto listener = TuyaDatapointListener{
      .datapoint_id = datapoint_id,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      func(datapoint);
  }
}

}  // namespace tuya
}  // namespace esphome
