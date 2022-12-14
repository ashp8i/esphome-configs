#include "esphome/core/log.h"
#include "tuyacovercustom.h"

namespace esphome {
namespace tuyacovercustom {

static const char *TAG = "tuyacovercustom";

// protocol
// #define ZM79EDT_MAX_LEN 256  // Max length of message value
#define ZM79EDT_BUFFER_LEN 6 // Length of serial buffer for header + type + length
#define ZM79EDT_HEADER_LEN 2 // Length of fixed header

// enable/disable reversed motor direction
#define ZM79EDT_ENABLE_REVERSING { 0x05, 0x01, 0x00, 0x01, 0x01 } //dpid = 5, type = bool, len = 1, value = enable
#define ZM79EDT_DISABLE_REVERSING { 0x05, 0x01, 0x00, 0x01, 0x00 } //dpid = 5, type = bool, len = 1, value = disable
// Curtain commands
#define ZM79EDT_OPEN { 0x01, 0x04, 0x00, 0x01, 0x02 } //dpid = 1, type = enum, len = 1, value = CLOSE
#define ZM79EDT_CLOSE { 0x01, 0x04, 0x00, 0x01, 0x00 } //dpid = 1, type = enum, len = 1, value = OPEN
#define ZM79EDT_STOP { 0x01, 0x04, 0x00, 0x01, 0x01 } //dpid = 1, type = enum, len = 1, value = STOP

// set position percentage
#define ZM79EDT_SET_POSITION { 0x02, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00 } //"02020004000000" //dpid = 2, type = value, len = 4,  value = 0x000000 + 1 byte (0x00-0x64)

static const char *TAG = "ZM79EDT";
static const uint16_t ZM79EDT_HEADER = 0x55AA;
//static const uint16_t ZM79EDT_VERSION = 0x03;
static const uint16_t ZM79EDT_VERSION = 0x00;

const uint8_t zm79edt_enable_reversing[] = ZM79EDT_ENABLE_REVERSING;
const uint8_t zm79edt_disable_reversing[] = ZM79EDT_DISABLE_REVERSING;
const uint8_t zm79edt_open[] = ZM79EDT_OPEN;
const uint8_t zm79edt_close[] = ZM79EDT_CLOSE;
const uint8_t zm79edt_stop[] = ZM79EDT_STOP;
static uint8_t zm79edt_pos[] = ZM79EDT_SET_POSITION;

#define HEARTBEAT_INTERVAL_MS 10000
unsigned long previousHeartbeatMillis = 0;

struct ZM79EDTCommand
{
    uint16_t header;
    uint8_t version;
    uint8_t command;
    uint16_t length;
    uint8_t value[ZM79EDT_MAX_LEN];
    uint8_t checksum;
};

struct ZM79EDTMessage
{
    uint8_t dpid;
    uint8_t type;
    uint16_t len;
    uint8_t value[ZM79EDT_MAX_LEN - 4]; //Subtract dpid, type, len
};

enum ZM79EDTCommandType
{
    ZM79EDT_HEARTBEAT = 0x00,
    ZM79EDT_COMMAND = 0x06,
    ZM79EDT_RESPONSE = 0x07,
    ZM79EDT_QUERY_STATUS = 0x08
};

enum ZM79EDTdpidType
{
    ZM79EDT_DPID_POSITION= 0x03,
    ZM79EDT_DPID_DIRECTION = 0x05,
    ZM79EDT_DPID_UNKNOWN = 0x07,
    ZM79EDT_DPID_ERROR = 0x0A
};

// Variables
ZM79EDTCommand command_{ZM79EDT_HEADER, ZM79EDT_VERSION, 0, 0, {}, 0};
uint8_t uart_buffer_[ZM79EDT_BUFFER_LEN]{0};

// Forward declarations
bool read_command();
void write_command(ZM79EDTCommandType command, const uint8_t *value, uint16_t length);
uint8_t checksum();

/*
     * Attempt to read an entire command from the serial UART into the command struct.
     * Will fail early if unable to find the two-byte header in the current
     * data stream. If the header is found, it will contine to read the complete
     * TLV+checksum sequence off the port. If the entire sequence can be read
     * and the checksum is valid, it will return true.
     */
bool read_command()
{
    // Shift bytes through until we find a valid header
    bool valid_header = false;
    while (Serial.available() >= 1)
    {
        uart_buffer_[0] = uart_buffer_[1];
        uart_buffer_[1] = Serial.read();
        command_.header = (uart_buffer_[0] << 8) + uart_buffer_[1];
        if (command_.header == ZM79EDT_HEADER)
        {
            valid_header = true;
            break;
        }
    }

    // Read the next 4 bytes (Version, Command, Data length)
    // Read n bytes (Data length)
    // Read the checksum byte
    if (valid_header)
    {
        Serial.readBytes(uart_buffer_ + ZM79EDT_HEADER_LEN, ZM79EDT_BUFFER_LEN - ZM79EDT_HEADER_LEN);
        command_.version = uart_buffer_[2];
        command_.command = uart_buffer_[3];
        command_.length = (uart_buffer_[4] << 8) + uart_buffer_[5];
        ESP_LOGV(TAG, "RX: Header = 0x%04X, Version = 0x%02X, Command = 0x%02X, Data length = 0x%04X", command_.header, command_.version, command_.command, command_.length);

        if (command_.length < ZM79EDT_MAX_LEN)
        {
            Serial.readBytes(command_.value, command_.length);
            ESP_LOGV(TAG, "RX_RAW:");
            for (size_t i = 0; i < command_.length; i++)
            {
                ESP_LOGV(TAG, "%02d: 0x%02X", i, command_.value[i]);
            }
            while (Serial.available() == 0) // Dirty
            {
                //Wait 
            }
            command_.checksum = Serial.read();
            
            ESP_LOGV(TAG, "RX_CHK: 0x%02X", command_.checksum);
            uint8_t calc_checksum = checksum();
            if (calc_checksum == command_.checksum)
            {
                // Clear buffer contents to start with beginning of next command
                memset(uart_buffer_, 0, ZM79EDT_BUFFER_LEN);
                return true;
            }
            else
            {
                memset(uart_buffer_, 0, ZM79EDT_BUFFER_LEN);
                ESP_LOGE(TAG, "Checksum error: Read = 0x%02X != Calculated = 0x%02X", command_.checksum, calc_checksum);
            }
        }
        else
        {
            memset(uart_buffer_, 0, ZM79EDT_BUFFER_LEN);
            ESP_LOGE(TAG, "Command length exceeds limit: %d >= %d", command_.length, ZM79EDT_MAX_LEN);
        }
    }

    // Do not clear buffer to allow for resume in case of reading partway through header RX
    return false;
}

/*
     * Store the given type, value, and length into the command struct and send
     * it out the serial port. Automatically calculates the checksum as well.
     */
void write_command(ZM79EDTCommandType command, const uint8_t *value, uint16_t length)
{
    // Copy params into command struct
    command_.header = ZM79EDT_HEADER;
    command_.version = ZM79EDT_VERSION;
    command_.command = command;
    command_.length = length;
    ESP_LOGV(TAG, "TX: Header = 0x%04X, Version = 0x%02X, Command = 0x%02X, Data length = 0x%04X", command_.header, command_.version, command_.command, command_.length);
    memcpy(&command_.value, value, length);
    ESP_LOGV(TAG, "TX_RAW");
    for (size_t i = 0; i < command_.length; i++)
    {
        ESP_LOGV(TAG, "%02d: 0x%02X", i, command_.value[i]);
    }
    // Copy struct values into buffer, converting longs to big-endian
    uart_buffer_[0] = command_.header >> 8;
    uart_buffer_[1] = command_.header & 0xFF;
    uart_buffer_[2] = command_.version;
    uart_buffer_[3] = command_.command;
    uart_buffer_[4] = command_.length >> 8;
    uart_buffer_[5] = command_.length & 0xFF;
    command_.checksum = checksum();
    ESP_LOGV(TAG, "TX_CHK: 0x%02X", command_.checksum);
    // Send buffer out via UART
    Serial.write(uart_buffer_, ZM79EDT_BUFFER_LEN);
    Serial.write(command_.value, command_.length);
    Serial.write(command_.checksum);
    // Clear buffer contents to avoid re-reading our own payload
    memset(uart_buffer_, 0, ZM79EDT_BUFFER_LEN);
}

/*
     * Calculate checksum from current UART buffer (header+type+length) plus command value.
     */
uint8_t checksum()
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < ZM79EDT_BUFFER_LEN; i++)
    {
        checksum += uart_buffer_[i];
    }
    for (size_t i = 0; i < command_.length; i++)
    {
        checksum += command_.value[i];
    }
    return checksum;
}

}  // namespace empty_uart_component
}  // namespace esphome