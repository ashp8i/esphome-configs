/*
 * VEML6075.cpp
 *
 * Arduino library for the Vishay VEML6075 UVA/UVB i2c sensor.
 *
 * Author: Sean Caulfield <sean@yak.net>
 * 
 * License: GPLv2.0
 *
 */

#include "VEML6075_1Wire.h"
#include "DS28E17.h"

VEML6075::VEML6075(OneWire *ow) {
 //_veml6075_onewire = oneWire;
 oneWire = ow;
    ds28e17 = DS28E17(ow);

  // Despite the datasheet saying this isn't the default on startup, it appears
  // like it is. So tell the thing to actually start gathering data.
  this->config = 0;
  this->config |= VEML6075_CONF_SD_OFF;

  // App note only provided math for this one, so be advised that changing it
  // will give you "undefined" results from all the calculations.
  // Might be able to do a linear compensation for the integration time length?
  this->config |= VEML6075_CONF_IT_100MS;
}

bool VEML6075::begin(const uint8_t* uuid) {


 deviceAddress = uuid;
    // init I2C over OneWire
 //_veml6075_onewire->select(deviceAddress);
 ds28e17.setAddress(deviceAddress);

  if (this->getDevID() != VEML6075_DEVID) {
    return false;
  }

  // Write config to make sure device is enabled
  this->write16(VEML6075_REG_CONF, this->config);


  return true;
}

// Poll sensor for latest values and cache them
void VEML6075::poll() {
  this->raw_uva = this->read16(VEML6075_REG_UVA);
  this->raw_uvb = this->read16(VEML6075_REG_UVB);
  this->raw_dark = this->read16(VEML6075_REG_DUMMY);
  this->raw_vis = this->read16(VEML6075_REG_UVCOMP1);
  this->raw_ir = this->read16(VEML6075_REG_UVCOMP2);
}

uint16_t VEML6075::getRawUVA() {
  return this->raw_uva;
}

uint16_t VEML6075::getRawUVB() {
  return this->raw_uvb;
}

uint16_t VEML6075::getRawDark() {
  return this->raw_dark;
}

uint16_t VEML6075::getRawVisComp() {
  return this->raw_vis;
}

uint16_t VEML6075::getRawIRComp() {
  return this->raw_ir;
}


uint16_t VEML6075::getDevID() {
  return this->read16(VEML6075_REG_DEVID);
}

float VEML6075::getUVA() {
  float comp_vis;
  float comp_ir;
  float comp_uva;

  // Constrain compensation channels to positive values
  comp_ir  = max(this->raw_ir - this->raw_dark, 0);
  comp_vis = max(this->raw_vis - this->raw_dark, 0);
  comp_uva = max(this->raw_uva - this->raw_dark, 0);

  // Scale by coeffs from datasheet
  comp_vis *= VEML6075_UVI_UVA_VIS_COEFF;
  comp_ir  *= VEML6075_UVI_UVA_IR_COEFF;

  // Subtract out visible and IR components
  comp_uva = max(comp_uva - comp_ir, 0.0F);
  comp_uva = max(comp_uva - comp_vis, 0.0F);

  return comp_uva;
}

float VEML6075::getUVB() {
  float comp_vis;
  float comp_ir;
  float comp_uvb;

  // Constrain compensation channels to positive values
  comp_ir  = max(this->raw_ir - this->raw_dark, 0);
  comp_vis = max(this->raw_vis - this->raw_dark, 0);
  comp_uvb = max(this->raw_uvb - this->raw_dark, 0);

  // Scale by coeffs from datasheet
  comp_vis *= VEML6075_UVI_UVB_VIS_COEFF;
  comp_ir  *= VEML6075_UVI_UVB_IR_COEFF;

  // Subtract out visible and IR components
  comp_uvb = max(comp_uvb - comp_ir, 0.0F);
  comp_uvb = max(comp_uvb - comp_vis, 0.0F);

  return comp_uvb;
}

float VEML6075::getUVIndex() {
  float uva_weighted = this->getUVA() * VEML6075_UVI_UVA_RESPONSE;
  float uvb_weighted = this->getUVB() * VEML6075_UVI_UVB_RESPONSE;
  return (uva_weighted + uvb_weighted) / 2.0;
}

uint16_t VEML6075::read16(uint8_t reg)
{
  uint8_t arr[2];
  uint16_t value;

  last_status = ds28e17.memoryRead(VEML6075_ADDR,reg, arr, 2);

  value  = (uint16_t)arr[1] << 8; // value high byte
  value |=           arr[0];      // value low byte

  return value;
}

void VEML6075::write16(uint8_t reg, uint16_t value)
{
  uint8_t data[3];
  data[0] = reg;
  data[1] = (value >> 8) & 0xFF; // value high byte
  data[2] = value & 0xFF; // value low byte
  last_status = ds28e17.write(VEML6075_ADDR, data, 3);
}