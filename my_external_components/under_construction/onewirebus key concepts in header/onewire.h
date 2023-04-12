#pragma once

#include "esphome/core/component.h"
#include <OneWire.h>

namespace esphome {
namespace onewire {

class OneWireComponent : public Component {
  public: 
    void setup() override;
    void dump_config() override;
    float get_setup_priority() const override { return esphome::setup_priority::DATA; }
  
  protected:
    OneWire *onewire_ = nullptr;
};

}  // namespace onewire 
}  // namespace esphome

#pragma once

#include "esphome/core/component.h"
#include <Arduino.h>

namespace esphome {
namespace onewire {

struct ROMDevice {
  uint8_t rom[8]; 
};

class OneWireComponent : public Component {
  public:
    OneWireComponent(uint8_t pin);

    void setup() override;
    void dump_config() override;
    float get_setup_priority() const override { return esphome::setup_priority::DATA; }

    uint8_t* search_rom();
    bool parasite_read(uint8_t *scratchpad);
    bool parasite_write(const uint8_t *scratchpad);
  
  protected:
    uint8_t pin_;
    void *ow_;
};

}  // namespace onewire  
}  // namespace esphome
onewire.cpp:

#include "onewire.h"
#define OW_PIN  pin_  

OneWireComponent::OneWireComponent(uint8_t pin) : pin_(pin) {}  

void OneWireComponent::setup() {   
  ow_ = onewire_setup(pin_, OW_PIN); 
}

uint8_t* OneWireComponent::search_rom() { /* ... */ }

bool OneWireComponent::parasite_read(uint8_t *scratchpad) { /* ... */ } 
bool OneWireComponent::parasite_write(const uint8_t *scratchpad) { /* ... */ }