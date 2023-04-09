#include <OneWire.h>
#include <Adafruit_Sensor.h>
#include <BME280_OneWire.h>

#define ONEWIRE_PIN 31

// Use Sample Sketch "OneWire_SearchDevice" to search for OneWire Devices
// Use the addresses you got here...
DeviceAddress addrS1 = {0x19,0x7D,0xDA,0x02,0x00,0x00,0x00,0x01};
DeviceAddress addrS2 = {0x19,0x4E,0xA3,0x02,0x00,0x00,0x00,0x2B};
OneWire oneWire(ONEWIRE_PIN);
BME280_OneWire Sensor1(&oneWire);
BME280_OneWire Sensor2(&oneWire);

void setup() {
    Serial.begin(19200);
    Serial.println("boot...");
    Sensor1.begin(addrS1);
    Sensor2.begin(addrS2);
    delay(500);
}

void loop() {
    printValues();
    delay(5000);
}

void printValues() {
    Sensor1.takeForcedMeasurement();
    Sensor2.takeForcedMeasurement();
    Serial.print("Temperature1 = ");
    Serial.print(" ");
    Serial.print(Sensor1.readTemperature());
    Serial.println(" *C");

    Serial.print("Pressure1 = ");
    Serial.print(" ");
    Serial.print(Sensor1.readPressure() / 100.0F);
    Serial.println(" hPa");

    Serial.print("Humidity1 = ");
    Serial.print(" ");
    Serial.print(Sensor1.readHumidity());
    Serial.println(" %");
    Serial.println();

    
    Serial.println();
    Serial.print("Temperature2 = ");
    Serial.print(" ");
    Serial.print(Sensor2.readTemperature());
    Serial.println(" *C");

    Serial.print("Pressure2 = ");
    Serial.print(" ");
    Serial.print(Sensor2.readPressure() / 100.0F);
    Serial.println(" hPa");

    Serial.print("Humidity2 = ");
    Serial.print(" ");
    Serial.print(Sensor2.readHumidity());
    Serial.println(" %");
    Serial.println();
    Serial.println();
}