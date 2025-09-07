/***************************************************************************
  This is a library for the BME280 humidity, temperature & pressure sensor

  Designed specifically to work with the Adafruit BME280 Breakout
  ----> http://www.adafruit.com/products/2650

  These sensors use I2C or SPI to communicate, 2 or 4 pins are required
  to interface.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!

  Written by Limor Fried & Kevin Townsend for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ***************************************************************************/
#include "Arduino.h"
#include <Wire.h>
#include <SPI.h>
#include "BME280_OneWire.h"

/***************************************************************************
 PRIVATE FUNCTIONS
 ***************************************************************************/
BME280_OneWire::BME280_OneWire(OneWire* _oneWire) {
	_bme280_onewire = _oneWire;
}


/**************************************************************************/
/*!
    @brief  Initialise sensor with given parameters / settings
*/
/**************************************************************************/
bool BME280_OneWire::begin(uint8_t* uuid)
{
	deviceAddress = uuid;
    // init I2C over OneWire
	_bme280_onewire->select(deviceAddress);

    // check if sensor, i.e. the chip ID is correct
    if(!isConnected())
        return false;

    // reset the device using soft-reset
    // this makes sure the IIR is off, etc.
    write8(BME280_REGISTER_SOFTRESET, 0xB6);

	
    // wait for chip to wake up.
    delay(300);

    // if chip is still reading calibration, delay
    while (isReadingCalibration())
          delay(100);

    readCoefficients(); // read trimming parameters, see DS 4.2.2

    setSampling(); // use defaults

    return true;
}

bool BME280_OneWire::isConnected() {
    if(read8(BME280_REGISTER_CHIPID) != 0x60)
        return false;
	return true;
}

/**************************************************************************/
/*!
    @brief  setup sensor with given parameters / settings
    
    This is simply a overload to the normal begin()-function, so SPI users
    don't get confused about the library requiring an address.
*/
/**************************************************************************/


void BME280_OneWire::setSampling(sensor_mode       mode,
		 sensor_sampling   tempSampling,
		 sensor_sampling   pressSampling,
		 sensor_sampling   humSampling,
		 sensor_filter     filter,
		 standby_duration  duration) {
    _measReg.mode     = mode;
    _measReg.osrs_t   = tempSampling;
    _measReg.osrs_p   = pressSampling;
        
    
    _humReg.osrs_h    = humSampling;
    _configReg.filter = filter;
    _configReg.t_sb   = duration;

    
    // you must make sure to also set REGISTER_CONTROL after setting the
    // CONTROLHUMID register, otherwise the values won't be applied (see DS 5.4.3)
    write8(BME280_REGISTER_CONTROLHUMID, _humReg.get());
    write8(BME280_REGISTER_CONFIG, _configReg.get());
    write8(BME280_REGISTER_CONTROL, _measReg.get());
}

/**************************************************************************/
/*!
    @brief  Writes an 8 bit value over onewire-I2C
*/
/**************************************************************************/
void BME280_OneWire::write8(byte reg, byte value) {
	uint8_t i = 0;
    uint8_t packet[7];                                		  // Reserve bytes to transmit data
    packet[0] = DS28E17_WRITE_DATA_WITH_STOP;         		  // Command ("Write Data With Stop")
    packet[1] = BME280_ADDRESS;                       		  // I2C slave to address
    packet[2] = 0x02;                                 		  // Number of data bytes to write
    packet[3] = reg;                     	          		  // The data to write
    packet[4] = value;                     		     		  // set status register

    uint16_t CRC16 = _bme280_onewire->crc16(&packet[0], 5);

    CRC16 = ~CRC16;
    packet[6] = CRC16 >> 8;                          		  // Most significant byte of 16 bit CRC
    packet[5] = CRC16 & 0xFF;                         		  // Least significant byte of 16 bit CRC

    _bme280_onewire->reset();
    _bme280_onewire->select(deviceAddress);
    _bme280_onewire->write_bytes(packet, sizeof(packet), 1);  // Write the packet and hold the bus
    while(i < TIMEOUT && _bme280_onewire->read_bit() == true) {              // Wait for not busy
        delay(1);
		i++;
    }
	
	_bme280_onewire->read();								  // Read DS28E17 Status
	_bme280_onewire->read();								  // Read DS28E17 Write Status
    _bme280_onewire->depower();                               // Release the bus
}


/**************************************************************************/
/*!
    @brief  Reads an 8 bit value over onewire-I2C
*/
/**************************************************************************/
uint8_t BME280_OneWire::read8(byte reg) {
	uint8_t i = 0;
    uint8_t value;
    uint8_t packet[7];                                		  // Reserve bytes to transmit data
    packet[0] = DS28E17_WRITE_READ_DATA_WITH_STOP;    		  // Command ("Write Data With Stop")
    packet[1] = BME280_ADDRESS;                       		  // I2C slave to address
    packet[2] = 0x01;                                 		  // Number of data bytes to write
    packet[3] = reg;                                  		  // The data to write
    packet[4] = 1;                                    		  // Number of bytes to read
    
    uint16_t CRC16 = _bme280_onewire->crc16(&packet[0], 5);

    CRC16 = ~CRC16;
    packet[6] = CRC16 >> 8;                           		  // Most significant byte of 16 bit CRC
    packet[5] = CRC16 & 0xFF;                         		  // Least significant byte of 16 bit CRC

    _bme280_onewire->reset();
    _bme280_onewire->select(deviceAddress);
    _bme280_onewire->write_bytes(packet, sizeof(packet), 1);  // Write the packet and hold the bus
    while(i < TIMEOUT && _bme280_onewire->read_bit() == true) {              // Wait for not busy
        delay(1);
		i++;
    }
	_bme280_onewire->read();								  // Read DS28E17 Status
	_bme280_onewire->read();								  // Read DS28E17 Write Status

    value = _bme280_onewire->read();						  // Read value
    _bme280_onewire->depower();                               // Release the bus
    return value;
}


/**************************************************************************/
/*!
    @brief  Reads a 16 bit value over I2C or SPI
*/
/**************************************************************************/
uint16_t BME280_OneWire::read16(byte reg)
{
	uint8_t i = 0;
    uint16_t value;
    uint8_t packet[7];                                		  // Reserve bytes to transmit data
    packet[0] = DS28E17_WRITE_READ_DATA_WITH_STOP;      	  // Command ("Write Data With Stop")
    packet[1] = BME280_ADDRESS;                       		  // I2C slave to address
    packet[2] = 0x01;                                 		  // Number of data bytes to write
    packet[3] = reg;                                  		  // The data to write
    packet[4] = 2;                                    		  // Number of bytes to read
    
    uint16_t CRC16 = _bme280_onewire->crc16(&packet[0], 5);

    CRC16 = ~CRC16;
    packet[6] = CRC16 >> 8;                           		  // Most significant byte of 16 bit CRC
    packet[5] = CRC16 & 0xFF;                         		  // Least significant byte of 16 bit CRC

    _bme280_onewire->reset();
    _bme280_onewire->select(deviceAddress);
    _bme280_onewire->write_bytes(packet, sizeof(packet), 1);   // Write the packet and hold the bus
    while(i < TIMEOUT && _bme280_onewire->read_bit() == true) {               // Wait for not busy
        delay(1);
		i++;
    }
	
	_bme280_onewire->read();								   // Read DS28E17 Status
	_bme280_onewire->read();								   // Read DS28E17 Write Status

	value = _bme280_onewire->read();						   // Read value
	value <<= 8;
	value |= _bme280_onewire->read();
    _bme280_onewire->depower();                                // Release the bus
    return value;
}


/**************************************************************************/
/*!
    
*/
/**************************************************************************/
uint16_t BME280_OneWire::read16_LE(byte reg) {
    uint16_t temp = read16(reg);
    return (temp >> 8) | (temp << 8);
}


/**************************************************************************/
/*!
    @brief  Reads a signed 16 bit value over I2C or SPI
*/
/**************************************************************************/
int16_t BME280_OneWire::readS16(byte reg)
{
    return (int16_t)read16(reg);
}


/**************************************************************************/
/*!
   
*/
/**************************************************************************/
int16_t BME280_OneWire::readS16_LE(byte reg)
{
    return (int16_t)read16_LE(reg);
}


/**************************************************************************/
/*!
    @brief  Reads a 24 bit value over I2C
*/
/**************************************************************************/
uint32_t BME280_OneWire::read24(byte reg)
{
	uint8_t i = 0;
    uint32_t value;
    uint8_t packet[7];                                		  // Reserve bytes to transmit data
    packet[0] = DS28E17_WRITE_READ_DATA_WITH_STOP;    		  // Command ("Write Data With Stop")
    packet[1] = BME280_ADDRESS;                       		  // I2C slave to address
    packet[2] = 0x01;                                 		  // Number of data bytes to write
    packet[3] = reg;                                  		  // The data to write
    packet[4] = 3;                                    		  // Number of bytes to read
    
    uint16_t CRC16 = _bme280_onewire->crc16(&packet[0], 5);

    CRC16 = ~CRC16;
    packet[6] = CRC16 >> 8;                           		  // Most significant byte of 16 bit CRC
    packet[5] = CRC16 & 0xFF;                         		  // Least significant byte of 16 bit CRC

    _bme280_onewire->reset();
    _bme280_onewire->select(deviceAddress);
    _bme280_onewire->write_bytes(packet, sizeof(packet), 1);   // Write the packet and hold the bus
    while(i < TIMEOUT && _bme280_onewire->read_bit() == true) {               // Wait for not busy
        delay(1);
		i++;
    }
	_bme280_onewire->read();								   // Read DS28E17 Status
	_bme280_onewire->read();								   // Read DS28E17 Write Status

	value = _bme280_onewire->read();						   // Read value
	value <<= 8;
	value |= _bme280_onewire->read();
	value <<= 8;
	value |= _bme280_onewire->read();
	_bme280_onewire->depower();                                // Release the bus
    return value;
}


/**************************************************************************/
/*!
    @brief  Take a new measurement (only possible in forced mode)
*/
/**************************************************************************/
bool BME280_OneWire::takeForcedMeasurement()
{   
	bool return_value = false;
	// If we are in forced mode, the BME sensor goes back to sleep after each
	// measurement and we need to set it to forced mode once at this point, so
	// it will take the next measurement and then return to sleep again.
	// In normal mode simply does new measurements periodically.
	if (_measReg.mode == MODE_FORCED) {
		return_value = true;
		// set to forced mode, i.e. "take next measurement"
		write8(BME280_REGISTER_CONTROL, _measReg.get());
		// Store current time to measure the timeout
		uint32_t timeout_start = millis();
		// wait until measurement has been completed, otherwise we would read the
		// the values from the last measurement or the timeout occurred after 2 sec.
		while (read8(BME280_REGISTER_STATUS) & 0x08) {
		  // In case of a timeout, stop the while loop
		  if ((millis() - timeout_start) > 2000) {
			return_value = false;
			break;
		  }
		  delay(1);
		}
	}
	return return_value;
}


/**************************************************************************/
/*!
    @brief  Reads the factory-set coefficients
*/
/**************************************************************************/
void BME280_OneWire::readCoefficients(void)
{
    _bme280_calib.dig_T1 = read16_LE(BME280_REGISTER_DIG_T1);
    _bme280_calib.dig_T2 = readS16_LE(BME280_REGISTER_DIG_T2);
    _bme280_calib.dig_T3 = readS16_LE(BME280_REGISTER_DIG_T3);

    _bme280_calib.dig_P1 = read16_LE(BME280_REGISTER_DIG_P1);
    _bme280_calib.dig_P2 = readS16_LE(BME280_REGISTER_DIG_P2);
    _bme280_calib.dig_P3 = readS16_LE(BME280_REGISTER_DIG_P3);
    _bme280_calib.dig_P4 = readS16_LE(BME280_REGISTER_DIG_P4);
    _bme280_calib.dig_P5 = readS16_LE(BME280_REGISTER_DIG_P5);
    _bme280_calib.dig_P6 = readS16_LE(BME280_REGISTER_DIG_P6);
    _bme280_calib.dig_P7 = readS16_LE(BME280_REGISTER_DIG_P7);
    _bme280_calib.dig_P8 = readS16_LE(BME280_REGISTER_DIG_P8);
    _bme280_calib.dig_P9 = readS16_LE(BME280_REGISTER_DIG_P9);

    _bme280_calib.dig_H1 = read8(BME280_REGISTER_DIG_H1);
    _bme280_calib.dig_H2 = readS16_LE(BME280_REGISTER_DIG_H2);
    _bme280_calib.dig_H3 = read8(BME280_REGISTER_DIG_H3);
    _bme280_calib.dig_H4 = (read8(BME280_REGISTER_DIG_H4) << 4) | (read8(BME280_REGISTER_DIG_H4+1) & 0xF);
    _bme280_calib.dig_H5 = (read8(BME280_REGISTER_DIG_H5+1) << 4) | (read8(BME280_REGISTER_DIG_H5) >> 4);
    _bme280_calib.dig_H6 = (int8_t)read8(BME280_REGISTER_DIG_H6);
}

/**************************************************************************/
/*!
    @brief return true if chip is busy reading cal data
*/
/**************************************************************************/
bool BME280_OneWire::isReadingCalibration(void)
{
  uint8_t const rStatus = read8(BME280_REGISTER_STATUS);

  return (rStatus & (1 << 0)) != 0;
}


/**************************************************************************/
/*!
    @brief  Returns the temperature from the sensor
*/
/**************************************************************************/
float BME280_OneWire::readTemperature()
{
	if(!isConnected())
        return DEVICE_DISCONNECTED;
    int32_t var1, var2;
    int32_t adc_T = read24(BME280_REGISTER_TEMPDATA);

    if (adc_T == 0x800000) // value in case temp measurement was disabled
        return NAN;
    adc_T >>= 4;

    var1 = ((((adc_T>>3) - ((int32_t)_bme280_calib.dig_T1 <<1))) *
            ((int32_t)_bme280_calib.dig_T2)) >> 11;
             
    var2 = (((((adc_T>>4) - ((int32_t)_bme280_calib.dig_T1)) *
              ((adc_T>>4) - ((int32_t)_bme280_calib.dig_T1))) >> 12) *
            ((int32_t)_bme280_calib.dig_T3)) >> 14;

    t_fine = var1 + var2;

    float T = (t_fine * 5 + 128) >> 8;
    return T/100;
}


/**************************************************************************/
/*!
    @brief  Returns the temperature from the sensor
*/
/**************************************************************************/
float BME280_OneWire::readPressure() {
    int64_t var1, var2, p;

    readTemperature(); // must be done first to get t_fine
	if(!isConnected())
		return DEVICE_DISCONNECTED;

    int32_t adc_P = read24(BME280_REGISTER_PRESSUREDATA);
    if (adc_P == 0x800000) // value in case pressure measurement was disabled
        return NAN;
    adc_P >>= 4;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)_bme280_calib.dig_P6;
    var2 = var2 + ((var1*(int64_t)_bme280_calib.dig_P5)<<17);
    var2 = var2 + (((int64_t)_bme280_calib.dig_P4)<<35);
    var1 = ((var1 * var1 * (int64_t)_bme280_calib.dig_P3)>>8) +
           ((var1 * (int64_t)_bme280_calib.dig_P2)<<12);
    var1 = (((((int64_t)1)<<47)+var1))*((int64_t)_bme280_calib.dig_P1)>>33;

    if (var1 == 0) {
        return 0; // avoid exception caused by division by zero
    }
    p = 1048576 - adc_P;
    p = (((p<<31) - var2)*3125) / var1;
    var1 = (((int64_t)_bme280_calib.dig_P9) * (p>>13) * (p>>13)) >> 25;
    var2 = (((int64_t)_bme280_calib.dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)_bme280_calib.dig_P7)<<4);
    return (float)p/256;
}


/**************************************************************************/
/*!
    @brief  Returns the humidity from the sensor
*/
/**************************************************************************/
float BME280_OneWire::readHumidity() {
    readTemperature(); // must be done first to get t_fine
	if(!isConnected())
        return DEVICE_DISCONNECTED;
	
    int32_t adc_H = read16(BME280_REGISTER_HUMIDDATA);
    if (adc_H == 0x8000) // value in case humidity measurement was disabled
        return NAN;
        
    int32_t v_x1_u32r;

    v_x1_u32r = (t_fine - ((int32_t)76800));

    v_x1_u32r = (((((adc_H << 14) - (((int32_t)_bme280_calib.dig_H4) << 20) -
                    (((int32_t)_bme280_calib.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
                 (((((((v_x1_u32r * ((int32_t)_bme280_calib.dig_H6)) >> 10) *
                      (((v_x1_u32r * ((int32_t)_bme280_calib.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
                    ((int32_t)2097152)) * ((int32_t)_bme280_calib.dig_H2) + 8192) >> 14));

    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                               ((int32_t)_bme280_calib.dig_H1)) >> 4));

    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    float h = (v_x1_u32r>>12);
    return  h / 1024.0;
}