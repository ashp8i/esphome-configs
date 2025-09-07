/*

DS28E17 1-Wire to i2C Bridge
============================

Arduino Library for Maxim Integrated DS28E17 1-Wire to i2C Bridge
Author: podija https://github.com/podija

Product: https://www.maximintegrated.com/en/products/interface/controllers-expanders/DS28E17.html

MIT License

*/
// #ifndef DS28E17_h
// #define DS28E17_h

// #include "Arduino.h"
// #include <OneWire.h>

// #define ONEWIRE_TIMEOUT 50

// #define DS28E17_ENABLE_SLEEP 0x1E
// #define DS28E17_WRITE 0x4B
// #define DS28E17_READ 0x87
// #define DS28E17_MEMMORY_READ 0x2D

// class DS28E17
// {
//   public:
//     DS28E17();      //Constructor of SoilSensor class.
//     DS28E17(OneWire *oneWire); //Constructor of SoilSensor class.
//     void setAddress(uint8_t *sensorAddress); //Set address (sensorAddress) of DS28E17.
//     void wakeUp();//Wake up asleep DS28E17.
//     void enableSleepMode();//Put DS28E17 into sleep mode.
//     bool write(uint8_t i2cAddress, uint8_t *data, uint8_t dataLength);
//     bool memoryWrite(uint8_t i2cAddress, uint8_t i2cRegister, uint8_t *data, uint8_t dataLength);
//     bool read(uint8_t i2cAddress, uint8_t *buffer, uint8_t bufferLength);
//     bool memoryRead(uint8_t i2cAddress, uint16_t i2cRegister, uint8_t *buffer, uint8_t bufferLength); 
    
//   private:
//     OneWire *oneWire;//Pointer to OneWire object.
//     uint8_t *address;//DS28E17 address.
//     bool _writeTo(uint8_t *header, uint8_t headerLength, uint8_t *data, uint8_t dataLength);
//     bool _readFrom(uint8_t *header, uint8_t headerLength, uint8_t *buffer, uint8_t bufferLength);       
// };

// #endif

#pragma once

#include "esphome.h"

#define ONEWIRE_TIMEOUT 50

#define DS28E17_ENABLE_SLEEP 0x1E
#define DS28E17_WRITE 0x4B
#define DS28E17_READ 0x87
#define DS28E17_MEMORY_READ 0x2D

namespace esphome {
namespace ds28e17 {

class DS28E17 : public esphome::Component
{
public:
    DS28E17(); // Constructor of DS28E17 class.
    DS28E17(esphome::OneWire &oneWire); // Constructor of DS28E17 class with OneWire reference.

    void setAddress(uint8_t *sensorAddress); // Set address (sensorAddress) of DS28E17.
    void wakeUp(); // Wake up asleep DS28E17.
    void enableSleepMode(); // Put DS28E17 into sleep mode.

    bool write(uint8_t i2cAddress, uint8_t *data, uint8_t dataLength);
    bool memoryWrite(uint8_t i2cAddress, uint8_t i2cRegister, uint8_t *data, uint8_t dataLength);
    bool read(uint8_t i2cAddress, uint8_t *buffer, uint8_t bufferLength);
    bool memoryRead(uint8_t i2cAddress, uint16_t i2cRegister, uint8_t *buffer, uint8_t bufferLength);

private:
    esphome::OneWire *oneWire; // Pointer to OneWire object.
    uint8_t *address; // DS28E17 address.
    bool _writeTo(uint8_t *header, uint8_t headerLength, uint8_t *data, uint8_t dataLength);
    bool _readFrom(uint8_t *header, uint8_t headerLength, uint8_t *buffer, uint8_t bufferLength);
};

} // namespace ds28e17
} // namespace esphome
