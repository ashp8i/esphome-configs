#include <OneWire.h>
#include <Adafruit_Sensor.h>
#include <BME280_OneWire.h>

#define ONEWIRE_PIN 19
OneWire oneWire(ONEWIRE_PIN);
BME280_OneWire Sensor(&oneWire);

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
      Sensor.begin(address);
      Sensor.takeForcedMeasurement();

      Serial.print("Data for Sensor ");
      Serial.println(count);
      Serial.print("Temperature = ");
      Serial.print(" ");
      Serial.print(Sensor.readTemperature());
      Serial.println(" *C");
  
      Serial.print("Pressure = ");
      Serial.print(" ");
      Serial.print(Sensor.readPressure() / 100.0F);
      Serial.println(" hPa");
  
      Serial.print("Humidity = ");
      Serial.print(" ");
      Serial.print(Sensor.readHumidity());
      Serial.println(" %");
      Serial.println();
    } while (oneWire.search(address));
  }
}