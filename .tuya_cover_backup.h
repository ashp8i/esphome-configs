#include "esphome.h"

// protocol
#define TUYA_COVER_MAX_LEN 256  // Max length of message value
#define TUYA_COVER_BUFFER_LEN 6 // Length of serial buffer for header + type + length
#define TUYA_COVER_HEADER_LEN 2 // Length of fixed header

// enable/disable reversed motor direction
// Normal = header (55AA) + (00060005) + 050100010011 "(55AA00060005050100010011)
// Reversed = header (55AA) + (00060005) + 050100010112 "(55AA00060005050100010112)"
#define TUYA_COVER_DISABLE_REVERSING { 0x05, 0x01, 0x00, 0x01, 0x00 } //dpid = 5, type = bool, len = 1, value = disable
#define TUYA_COVER_ENABLE_REVERSING { 0x05, 0x01, 0x00, 0x01, 0x01 } //dpid = 5, type = bool, len = 1, value = enable
// Curtain commands
// Close = header (55AA) + (00060005) + 660400010075 "(55aa00060005660400010075)"
//  Open = header (55AA) + (00060005) + 660400010076 "(55aa00060005660400010076)"
//  Stop = header (55AA) + (00060005) + 660400010077 "(55AA00060005660400010077)"
#define TUYA_COVER_OPEN { 0x66, 0x04, 0x00, 0x01, 0x01 } //dpid = 1, type = enum, len = 1, value = CLOSE
#define TUYA_COVER_CLOSE { 0x66, 0x04, 0x00, 0x01, 0x00 } //dpid = 1, type = enum, len = 1, value = OPEN
#define TUYA_COVER_STOP { 0x66, 0x04, 0x00, 0x01, 0x02 } //dpid = 1, type = enum, len = 1, value = STOP
// #define TUYA_COVER_OPEN { 0x66, 0x04, 0x00, 0x01, 0x00, 0x75 } //dpid = 1, type = enum, len = 1, value = CLOSE
// #define TUYA_COVER_CLOSE { 0x66, 0x04, 0x00, 0x01, 0x00, 0x76 } //dpid = 1, type = enum, len = 1, value = OPEN
// #define TUYA_COVER_STOP { 0x66, 0x04, 0x00, 0x01, 0x00, 0x77 } //dpid = 1, type = enum, len = 1, value = STOP

// set position percentage
// Position = header (55AA) + (00060005) + 0202000400000000 "(55aa0006000502020004000000000)"
// Position = header (55AA) + (00070008) + 6502000400000000 "(55aa000700086502000400000000)"
// Position = header (55AA) + (00070008) + 6502000400000000 "(55aa000700086502000400000064)"
// 55AA000700086502000400000000
// 55aa000600086502000400000005

// 55 AA 00 06 00 08 65 02 00 04 00 00 00 44 BC 
// 55 AA 00 06 00 08 65 02 00 04 00 00 00 57 CF 

// 21:15:37.611 RSL: RESULT = {"POWER":"ON","Dimmer":5}
// 21:15:37.699 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000000010101","Cmnd":0,"CmndData":"01"}}
// 21:15:37.702 TYA: Heartbeat
#define TUYA_COVER_SET_POSITION { 0x65, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00 } //"65020004000000" //dpid = 2, type = value, len = 4,  value = 0x000000 + 1 byte (0x00-0x64)

// OPEN
// TX: 55AA00060005660400010076
// TX: 55AA00060005010400010212
// CLOSE
// TX: 55AA00060005660400010075
// STOP
// TX: 55AA00060005660400010277

// dimmer position
// TX: 55AA000700086502000400000051

// 20:47:35.214 TYA: Send "55aa000600086502000400000051c9"
// 20:47:35.378 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000700086502000400000062DB","Cmn
// 20:48:40.431 TYA: Send "55aa000600086502000400000064dc"
// 20:48:40.442 TYA: Send "55aa00000000ff"
// 20:48:40.606 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000700086502000400000051CA","Cmnd":7,"CmndData":"6502000400000051","DpType2Id101":81,"101":{"DpId":101,"DpIdType":2,"DpIdData":"00000051"}}}
// 20:48:40.610 TYA: fnId=21 is set for dpId=101
// 20:48:40.612 TYA: RX value 81 from dpId 101 
// 20:48:40.618 RSL: RESULT = {"TuyaReceived":{"Data":"55AA00070005660400010278","Cmnd":7,"CmndData":"6604000102","DpType4Id102":2,"102":{"DpId":102,"DpIdType":4,"DpIdData":"02"}}}
// 20:48:40.621 TYA: fnId=0 is set for dpId=102
// 20:48:41.228 CFG: Saved to flash at F8, Count 108, Bytes 4096
// 20:48:43.626 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000700086502000400000064DD","Cmnd":7,"CmndData":"6502000400000064","DpType2Id101":100,"101":{"DpId":101,"DpIdType":2,"DpIdData":"00000064"}}}
// 20:48:43.629 TYA: fnId=21 is set for dpId=101
// 20:48:43.631 TYA: RX value 100 from dpId 101 
// 20:48:43.637 RSL: RESULT = {"TuyaReceived":{"Data":"55AA00070005660400010278","Cmnd":7,"CmndData":"6604000102","DpType4Id102":2,"102":{"DpId":102,"DpIdType":4,"DpIdData":"02"}}}
// 20:48:43.640 TYA: fnId=0 is set for dpId=102
// 20:48:44.411 RSL: STATE = {"Time":"2022-11-12T20:48:44","Uptime":"4T01:20:13","UptimeSec":350413,"Heap":22,"SleepMode":"Dynamic","Sleep":10,"LoadAvg":99,"MqttCount":0,"POWER":"ON","Dimmer":100,"Fade":"OFF","Speed":1,"LedTable":"ON","Wifi":{"AP":1,"SSId":"Matrix","BSSId":"F0:2F:74:C4:22:C8","Channel":3,"Mode":"11n","RSSI":72,"Signal":-64,"LinkCount":1,"Downtime":"0T00:00:03"}}
// 20:48:51.411 TYA: Send "55aa00000000ff"
// 20:48:51.521 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000000010101","Cmnd":0,"CmndData":"01"}}
// 20:48:51.523 TYA: Heartbeat


// TX: 55AA00060005010400010212
// RX: 55AA03070005010400010215
// RX: 55AA0307000803020004000000647E

// Close:
// TX: 55AA00060005010400010010
// RX: 55AA03070005010400010013 Included CRC = 0x13 calculated CRC = 0x14

// Stop:
// TX: 55AA00060005010400010111

// Control by remote or pull curtain

// Open/Stop/Close:
// RX: 55AA0307000507040001001A (same response for open/stop/close)
// RX: 55AA0307000803020004000000001A (Reports position on stop)

// 19:42:35.417 TYA: Send "55aa001c000801160b0c132a2306b7"
// 19:42:36.781 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000700086502000400000063DC","Cmnd":7,"CmndData":"6502000400000063","DpType2Id101":99,"101":{"DpId":101,"DpIdType":2,"DpIdData":"00000063"}}}
// 19:42:36.785 TYA: fnId=21 is set for dpId=101
// 19:42:36.787 TYA: RX value 99 from dpId 101 
// 19:42:36.790 RSL: RESULT = {"TuyaReceived":{"Data":"55AA00070005660400010177","Cmnd":7,"CmndData":"6604000101","DpType4Id102":1,"102":{"DpId":102,"DpIdType":4,"DpIdData":"01"}}}
// 19:42:36.793 TYA: fnId=0 is set for dpId=102
// 19:42:40.408 TYA: Send "55aa00000000ff"
// 19:42:40.517 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000000010101","Cmnd":0,"CmndData":"01"}}
// 19:42:40.519 TYA: Heartbeat
// 19:42:46.986 RSL: RESULT = {"TuyaReceived":{"Data":"55AA00070008650200040000000079","Cmnd":7,"CmndData":"6502000400000000","DpType2Id101":0,"101":{"DpId":101,"DpIdType":2,"DpIdData":"00000000"}}}
// 19:42:46.989 TYA: fnId=21 is set for dpId=101
// 19:42:46.991 TYA: RX value 0 from dpId 101 
// 19:42:46.997 RSL: RESULT = {"TuyaReceived":{"Data":"55AA00070005660400010278","Cmnd":7,"CmndData":"6604000102","DpType4Id102":2,"102":{"DpId":102,"DpIdType":4,"DpIdData":"02"}}}
// 19:42:46.999 TYA: fnId=0 is set for dpId=102
// 19:42:51.413 TYA: Send "55aa00000000ff"
// 19:42:51.522 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000000010101","Cmnd":0,"CmndData":"01"}}
// 19:42:51.524 TYA: Heartbeat
// 19:42:54.181 WIF: Checking connection...

// 19:43:30.431 TYA: RX value 1 from dpId 101 
// 19:43:30.433 SRC: Switch
// 19:43:30.436 CMD: Grp 0, Cmd 'DIMMER', Idx 3, Len 1, Pld 1, Data '1'
// 19:43:30.441 RSL: RESULT = {"POWER":"ON","Dimmer":1}
// 19:43:30.445 RSL: RESULT = {"TuyaReceived":{"Data":"55AA00070005660400010076","Cmnd":7,"CmndData":"6604000100","DpType4Id102":0,"102":{"DpId":102,"DpIdType":4,"DpIdData":"00"}}}
// 19:43:30.449 TYA: fnId=0 is set for dpId=102
// 19:43:31.231 CFG: Saved to flash at F6, Count 102, Bytes 4096
// 19:43:32.403 WIF: Sending Gratuitous ARP
// 19:43:34.434 WIF: Checking connection...
// 19:43:35.414 TYA: Send "55aa00000000ff"
// 19:43:35.431 TYA: Send "55aa001c000801160b0c132b2306b8"
// 19:43:35.541 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000000010101","Cmnd":0,"CmndData":"01"}}
// 19:43:35.543 TYA: Heartbeat
// 19:43:41.165 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000700086502000400000064DD","Cmnd":7,"CmndData":"6502000400000064","DpType2Id101":100,"101":{"DpId":101,"DpIdType":2,"DpIdData":"00000064"}}}
// 19:43:41.168 TYA: fnId=21 is set for dpId=101
// 19:43:41.170 TYA: RX value 100 from dpId 101 
// 19:43:41.172 SRC: Switch
// 19:43:41.175 CMD: Grp 0, Cmd 'DIMMER', Idx 3, Len 3, Pld 100, Data '100'
// 19:43:41.181 RSL: RESULT = {"POWER":"ON","Dimmer":100}
// 19:43:41.184 RSL: RESULT = {"TuyaReceived":{"Data":"55AA00070005660400010278","Cmnd":7,"CmndData":"6604000102","DpType4Id102":2,"102":{"DpId":102,"DpIdType":4,"DpIdData":"02"}}}
// 19:43:41.188 TYA: fnId=0 is set for dpId=102
// 19:43:41.236 CFG: Saved to flash at F5, Count 103, Bytes 4096
// 19:43:44.406 RSL: STATE = {"Time":"2022-11-12T19:43:44","Uptime":"4T00:15:13","UptimeSec":346513,"Heap":18,"SleepMode":"Dynamic","Sleep":10,"LoadAvg":99,"MqttCount":0,"POWER":"ON","Dimmer":100,"Fade":"OFF","Speed":1,"LedTable":"ON","Wifi":{"AP":1,"SSId":"Matrix","BSSId":"F0:2F:74:C4:22:C8","Channel":3,"Mode":"11n","RSSI":70,"Signal":-65,"LinkCount":1,"Downtime":"0T00:00:03"}}
// 19:43:46.405 TYA: Send "55aa00000000ff"
// 19:43:46.515 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000000010101","Cmnd":0,"CmndData":"01"}}
// 19:43:46.517 TYA: Heartbeat
// 19:43:54.430 WIF: Checking connection...
// 19:43:57.408 TYA: Send "55aa00000000ff"
// 19:43:57.518 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000000010101","Cmnd":0,"CmndData":"01"}}
// 19:43:57.519 TYA: Heartbeat

// 19:45:58.382 RSL: RESULT = {"SerialSend":"Done"}
// 19:45:58.475 TYA: Send "55aa00000000ff"
// 19:45:59.993 RSL: RESULT = {"TuyaReceived":{"Data":"55AA00070008650200040000004CC5","Cmnd":7,"CmndData":"650200040000004C","DpType2Id101":76,"101":{"DpId":101,"DpIdType":2,"DpIdData":"0000004C"}}}
// 19:45:59.997 TYA: fnId=21 is set for dpId=101
// 19:45:59.999 TYA: RX value 76 from dpId 101 
// 19:46:00.005 RSL: RESULT = {"TuyaReceived":{"Data":"55AA00070005660400010076","Cmnd":7,"CmndData":"6604000100","DpType4Id102":0,"102":{"DpId":102,"DpIdType":4,"DpIdData":"00"}}}
// 19:46:00.008 TYA: fnId=0 is set for dpId=102
// 19:46:03.535 RSL: RESULT = {"TuyaReceived":{"Data":"55AA000700086502000400000064DD","Cmnd":7,"CmndData":"6502000400000064","DpType2Id101":100,"101":{"DpId":101,"DpIdType":2,"DpIdData":"00000064"}}}
// 19:46:03.539 TYA: fnId=21 is set for dpId=101
// 19:46:03.541 TYA: RX value 100 from dpId 101 
// 19:46:03.543 SRC: Switch
// 19:46:03.545 CMD: Grp 0, Cmd 'DIMMER', Idx 3, Len 3, Pld 100, Data '100'
// 19:46:03.551 RSL: RESULT = {"POWER":"ON","Dimmer":100}
// 19:46:03.556 RSL: RESULT = {"TuyaReceived":{"Data":"55AA00070005660400010278","Cmnd":7,"CmndData":"6604000102","DpType4Id102":2,"102":{"DpId":102,"DpIdType":4,"DpIdData":"02"}}}
// 19:46:03.559 TYA: fnId=0 is set for dpId=102
// 19:46:04.225 CFG: Saved to flash at FB, Count 105, Bytes 4096



// heartbeat packet 55aa00000000ff
// gratuitous arp / 55aa001c000801160b0c121f2306ab
// RX 55AA000000010101
// RX 55AA00070005660100000101
// RX 55AA00070005660100000102
// TX 55aa000600086502000400000061d9
// RX 55AA00060005070100010013
// RX 55aa00060005070104000102
// RX 55AA00060005010400010212
// RX 55AA000600010101
// RX 55AA00070008650200040212

// Open:
// TX: 55AA00060005010400010212
// RX: 55AA03070005010400010215 Included CRC = 0x15 calculated CRC = 0x16

// Close:
// TX: 55AA00060005010400010010
// RX: 55AA03070005010400010013 Included CRC = 0x13 calculated CRC = 0x14

// Stop:
// TX: 55AA00060005010400010111
// RX: 55AA03070005010400010114 Included CRC = 0x14 calculated CRC = 0x15

// Boot sequence:

// 1) Detecting heartbeat
// TX: 55AA00000000FF
// RX: 55AA030000010003 (0x00 indicates the first heartbeat)

// 2) Querying product information
// TX: 55AA0001000000
// RX: 55AA00010015585337364259355131754B4F36676A43312E302E30C5
// Cmnd":1,"CmndData":"585337364259355131754B4F36676A43312E302E30"}}
// MCU Product ID: XS76BY5Q1uKO6gjC1.0.0
// TYA: Send "55aa0002000001"
// RSL: RESULT = {"TuyaReceived":{"Data":"55AA0002000001","Cmnd":2}}
// TYA: RX MCU configuration Mode=0
// TYA: Read MCU state
// TYA: Send "55aa0008000007"

// 3) Querying working mode
// TX: 55AA0002000001
// RX: 55AA0302000004 (0x0000 indicates coordination mode with MCU)

// 4) Querying Status (Async)
// TX: 55AA0008000007
// RX: 55AA03070005010400010216 (dpid 1 - curtain control)
// RX: 55AA0307000803020004000000001A (dpid 8 - motor direction)
// RX: 55AA03070005050100010116 (dpid 5 - motor direction)
// RX: 55AA0307000507040001011B (dpid 7 - unknown)
// RX: 55AA030700050A050001001E (dpid 10 - error report)

// 5) Reporting device network status (Wi-Fi has been configured but not connected to the router)
// TX: 55AA000300010205
// RX: 55AA0303000005

// 6) Reporting device network status (Wi-Fi has been configured and connected to the router.)
// TX: 55AA000300010306
// RX: 55AA0303000005

// 7) Reporting device network status (Wi-Fi has been connected to the router and the cloud.)
// TX: 55AA000300010407

// 6) Detecting heartbeat
// TX: 55AA00000000FF
// RX: 55AA030000010104
// Use cases:

// Control commands

// Open:
// TX: 55AA00060005010400010212
// RX: 55AA03070005010400010215
// RX: 55AA0307000803020004000000647E (Reports position on stop)

// Stop:
// TX: 55AA00060005010400010111
// RX: 55AA03070005010400010114
// RX: 55AA03070008030200040000002B42 (Reports position on stop)

// Close:
// TX: 55AA00060005010400010010
// RX: 55AA03070005010400010013
// RX: 55AA0307000803020004000000001A (Reports position on stop)

// Motor direction set normal operation:
// TX: 55AA00060005050100010011
// RX: 55AA03070005050100010015

// Motor direction set reversed operation:
// TX: 55AA00060005050100010112
// RX: 55AA03070005050100010116

// Control by remote or pull curtain

// Open/Stop/Close:
// RX: 55AA0307000507040001001A (same response for open/stop/close)
// RX: 55AA0307000803020004000000001A (Reports position on stop)

// Close + curtain blocked:
// RX: 55AA0307000507040001001A
// RX: 55AA030700050A0500010220
// RX: 55AA0307000803020004000000001A (Reports position on stop)

static const char *TAG = "TUYACOVER";
static const uint16_t TUYA_COVER_HEADER = 0x55AA;
//static const uint16_t TUYA_COVER_VERSION = 0x03;
static const uint16_t TUYA_COVER_VERSION = 0x00;

const uint8_t tuyacover_enable_reversing[] = TUYA_COVER_ENABLE_REVERSING;
const uint8_t tuyacover_disable_reversing[] = TUYA_COVER_DISABLE_REVERSING;
const uint8_t tuyacover_open[] = TUYA_COVER_OPEN;
const uint8_t tuyacover_close[] = TUYA_COVER_CLOSE;
const uint8_t tuyacover_stop[] = TUYA_COVER_STOP;
static uint8_t tuyacover_pos[] = TUYA_COVER_SET_POSITION;

#define HEARTBEAT_INTERVAL_MS 10000
unsigned long previousHeartbeatMillis = 0;

struct TUYACOVERCommand
{
    uint16_t header;
    uint8_t version;
    uint8_t command;
    uint16_t length;
    uint8_t value[TUYA_COVER_MAX_LEN];
    uint8_t checksum;
};

struct TUYACOVERMessage
{
    uint8_t dpid;
    uint8_t type;
    uint16_t len;
    uint8_t value[TUYA_COVER_MAX_LEN - 4]; //Subtract dpid, type, len
};

enum TUYACOVERCommandType
{
    TUYA_COVER_HEARTBEAT = 0x00,
    TUYA_COVER_COMMAND = 0x06,
    TUYA_COVER_RESPONSE = 0x07,
    TUYA_COVER_QUERY_STATUS = 0x08
};

enum TUYACOVERdpidType
{
    TUYA_COVER_DPID_POSITION= 0x03,
    TUYA_COVER_DPID_DIRECTION = 0x05,
    TUYA_COVER_DPID_UNKNOWN = 0x07,
    TUYA_COVER_DPID_ERROR = 0x0A
};

// Variables
TUYACOVERCommand command_{TUYA_COVER_HEADER, TUYA_COVER_VERSION, 0, 0, {}, 0};
uint8_t uart_buffer_[TUYA_COVER_BUFFER_LEN]{0};

// Forward declarations
bool read_command();
void write_command(TUYACOVERCommandType command, const uint8_t *value, uint16_t length);
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
        if (command_.header == TUYA_COVER_HEADER)
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
        Serial.readBytes(uart_buffer_ + TUYA_COVER_HEADER_LEN, TUYA_COVER_BUFFER_LEN - TUYA_COVER_HEADER_LEN);
        command_.version = uart_buffer_[2];
        command_.command = uart_buffer_[3];
        command_.length = (uart_buffer_[4] << 8) + uart_buffer_[5];
        ESP_LOGV(TAG, "RX: Header = 0x%04X, Version = 0x%02X, Command = 0x%02X, Data length = 0x%04X", command_.header, command_.version, command_.command, command_.length);

        if (command_.length < TUYA_COVER_MAX_LEN)
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
                memset(uart_buffer_, 0, TUYA_COVER_BUFFER_LEN);
                return true;
            }
            else
            {
                memset(uart_buffer_, 0, TUYA_COVER_BUFFER_LEN);
                ESP_LOGE(TAG, "Checksum error: Read = 0x%02X != Calculated = 0x%02X", command_.checksum, calc_checksum);
            }
        }
        else
        {
            memset(uart_buffer_, 0, TUYA_COVER_BUFFER_LEN);
            ESP_LOGE(TAG, "Command length exceeds limit: %d >= %d", command_.length, TUYA_COVER_MAX_LEN);
        }
    }

    // Do not clear buffer to allow for resume in case of reading partway through header RX
    return false;
}

/*
     * Store the given type, value, and length into the command struct and send
     * it out the serial port. Automatically calculates the checksum as well.
     */
void write_command(TUYACOVERCommandType command, const uint8_t *value, uint16_t length)
{
    // Copy params into command struct
    command_.header = TUYA_COVER_HEADER;
    command_.version = TUYA_COVER_VERSION;
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
    Serial.write(uart_buffer_, TUYA_COVER_BUFFER_LEN);
    Serial.write(command_.value, command_.length);
    Serial.write(command_.checksum);
    // Clear buffer contents to avoid re-reading our own payload
    memset(uart_buffer_, 0, TUYA_COVER_BUFFER_LEN);
}

/*
     * Calculate checksum from current UART buffer (header+type+length) plus command value.
     */
uint8_t checksum()
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < TUYA_COVER_BUFFER_LEN; i++)
    {
        checksum += uart_buffer_[i];
    }
    for (size_t i = 0; i < command_.length; i++)
    {
        checksum += command_.value[i];
    }
    return checksum;
}

class CustomCurtain : public Component, public Cover
{

protected:

public:
    void setup() override
    {
        ESP_LOGI(TAG, "TUYA_COVER_COMMAND = QUERY_STATUS");
        write_command(TUYA_COVER_QUERY_STATUS, 0, 0);
    }

    CoverTraits get_traits() override
    {
        auto traits = CoverTraits();
        traits.set_is_assumed_state(false);
        traits.set_supports_position(true);
        traits.set_supports_tilt(false);
        return traits;
    }

    void control(const CoverCall &call) override
    {
        if (call.get_position().has_value())
        {
            // Write pos (range 0-1) to cover
            // Cover pos (range 0x00-0x64) (closed - open)
            uint8_t pos = (*call.get_position() * 100);
            ESP_LOGV(TAG, "POS = %d", pos);

            switch (pos)
            {
            case 0:
                ESP_LOGI(TAG, "TUYA_COVER_COMMAND = CLOSE");
                write_command(TUYA_COVER_COMMAND, tuyacover_close, sizeof(tuyacover_close));
                break;
            case 100:
                ESP_LOGI(TAG, "TUYA_COVER_COMMAND = OPEN");
                write_command(TUYA_COVER_COMMAND, tuyacover_open, sizeof(tuyacover_open));
                break;
            default:
                tuyacover_pos[7] = pos;
                ESP_LOGI(TAG, "TUYA_COVER_COMMAND = POS = %d%%", pos);
                write_command(TUYA_COVER_COMMAND, tuyacover_pos, sizeof(tuyacover_pos));
                break;
            }
            // publish_state only when position is confirmed in loop()
        }
        if (call.get_stop())
        {
            // User requested cover stop
            ESP_LOGI(TAG, "TUYA_COVER_COMMAND = STOP");
            write_command(TUYA_COVER_COMMAND, tuyacover_stop, sizeof(tuyacover_stop));
        }
    }

    void loop() override
    {
        unsigned long currentHeartbeatMillis = millis();
        if (currentHeartbeatMillis - previousHeartbeatMillis >= HEARTBEAT_INTERVAL_MS)
        {
            previousHeartbeatMillis += HEARTBEAT_INTERVAL_MS;
            ESP_LOGI(TAG, "TUYA_COVER_COMMAND = HEARTBEAT");
            write_command(TUYA_COVER_HEARTBEAT, 0, 0);
        }

        bool have_message = read_command();

        if(have_message && command_.command == TUYA_COVER_HEARTBEAT)
        {
            ESP_LOGI(TAG, "TUYA_COVER_RESPONSE = %s", (command_.value[0] == 0) ? "FIRST_HEARTBEAT" : "HEARTBEAT");  
        }
        else if (have_message && command_.command == TUYA_COVER_RESPONSE)
        {
            switch (command_.value[0])
            {
            case TUYA_COVER_DPID_POSITION:
                ESP_LOGI(TAG, "TUYA_COVER_DPID_POSITION = %d%%", command_.value[7]);
                this->position = ((command_.value[7]) / 100.0f);
                this->publish_state();
                break;
            case TUYA_COVER_DPID_DIRECTION:
                ESP_LOGI(TAG, "TUYA_COVER_DPID_DIRECTION = 0x%02X", command_.value[4]);
                break;
            case TUYA_COVER_DPID_UNKNOWN:
                ESP_LOGI(TAG, "TUYA_COVER_DPID_UNKNOWN ENUM = 0x%02X", command_.value[4]);
                break;
            case TUYA_COVER_DPID_ERROR:
                ESP_LOGI(TAG, "TUYA_COVER_DPID_ERROR BITMAP = 0x%02X", command_.value[4]);
                break;
            default:
                break;
            }
        }
    }

};

class CustomAPI : public Component, public CustomAPIDevice
{
public:
    void setup() override
    {
        register_service(&CustomAPI::setStatusReport, "get_status_report");
        register_service(&CustomAPI::setMotorNormal, "set_motor_normal");
        register_service(&CustomAPI::setMotorReversed, "set_motor_reversed");
        register_service(&CustomAPI::sendCommand, "send_command", {"data"});
    }

    void setStatusReport()
    {
        ESP_LOGI(TAG, "TUYA_COVER_COMMAND = QUERY_STATUS");
        write_command(TUYA_COVER_QUERY_STATUS, 0, 0);
    }

    void setMotorNormal()
    {
        ESP_LOGI(TAG, "TUYA_COVER_COMMAND = ENABLE_REVERSING");
        write_command(TUYA_COVER_COMMAND, tuyacover_enable_reversing, sizeof(tuyacover_enable_reversing));
    }

    void setMotorReversed()
    {
        ESP_LOGI(TAG, "TUYA_COVER_COMMAND = ENABLE_REVERSING");
        write_command(TUYA_COVER_COMMAND, tuyacover_disable_reversing, sizeof(tuyacover_disable_reversing));
    }

    void sendCommand(std::string data)
    {
        int i = 0;
        uint8_t sum = 0;
        while (i < data.length())
        {
            const char hex[2] = {data[i++], data[i++]};
            uint8_t d = strtoul(hex, NULL, 16);
            sum += d;
            writeByte(d);
        }
        writeByte(sum);
    }

    void writeByte(uint8_t data)
    {
        Serial.write(data);
    }
};