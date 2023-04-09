#include <OneWire.h>
#include <Adafruit_Sensor.h>
#include <BME280_OneWire.h>

#define ONEWIRE_PIN 19

// Use Sample Sketch "OneWire_SearchDevice" to search for OneWire Devices
// Use the address you got here...
DeviceAddress addrS1 = {0x19,0x44,0xA2,0x03,0x00,0x00,0x00,0xA6};
OneWire oneWire(ONEWIRE_PIN);
BME280_OneWire Sensor1(&oneWire);

void setup() {
    Serial.begin(19200);
    Serial.println("boot...");
    Sensor1.begin(addrS1);
    delay(500);
}

void loop() {
    printValues();
    delay(5000);
}

void printValues() {
    Sensor1.takeForcedMeasurement();
    Serial.print("Temperature = ");
    Serial.print(" ");
    Serial.print(Sensor1.readTemperature());
    Serial.println(" *C");

    Serial.print("Pressure = ");
    Serial.print(" ");
    Serial.print(Sensor1.readPressure() / 100.0F);
    Serial.println(" hPa");

    Serial.print("Humidity = ");
    Serial.print(" ");
    Serial.print(Sensor1.readHumidity());
    Serial.println(" %");
    Serial.println();
}