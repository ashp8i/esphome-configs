#include <OneWire.h>
#include <Adafruit_Sensor.h>
#include <BME280_OneWire.h>

#define ONEWIRE_PIN 19
OneWire oneWire(ONEWIRE_PIN);

void setup() {
    Serial.begin(19200);
    Serial.println("boot...");
    delay(500);
}

void loop() {
    findDevices();
    delay(5000);
}

void findDevices() {
  uint8_t address[8];
  uint8_t count = 0;

  if(oneWire.search(address)) {
    do {
      count++;
      for (uint8_t i = 0; i < 8; i++) {
        Serial.print("0x");
        if(address[i] < 0x10) Serial.print("0");
        Serial.print(address[i], HEX);
        if(i < 7) Serial.print(",");
      }
    } while (oneWire.search(address));
    Serial.println();
    Serial.print("nr devices found: ");
    Serial.println(count);
  }
}