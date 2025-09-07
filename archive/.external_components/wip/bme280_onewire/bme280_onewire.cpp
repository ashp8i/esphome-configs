#include "esphome.h"
#include "BME280_OneWire.h"

class BME280OneWire : public PollingComponent, public Sensor {
 public:
  BME280OneWire(OneWire* one_wire, uint8_t* device_address, float update_interval)
      : PollingComponent(update_interval), one_wire_(one_wire), device_address_(device_address) {}

  void setup() override {
    bme280_ = new BME280_OneWire(one_wire_, device_address_);
    bme280_->begin();
  }

  void update() override {
    float temperature = bme280_->readTemperature();
    float humidity = bme280_->readHumidity();
    float pressure = bme280_->readPressure() / 100.0;

    ESP_LOGD("bme280_onewire", "Temperature: %.2f °C", temperature);
    ESP_LOGD("bme280_onewire", "Humidity: %.2f %%", humidity);
    ESP_LOGD("bme280_onewire", "Pressure: %.2f hPa", pressure);

    publish_state(temperature);
    publish_state(humidity);
    publish_state(pressure);
  }

 private:
  OneWire* one_wire_;
  uint8_t* device_address_;
  BME280_OneWire* bme280_;
};

class BME280OneWireSensor : public Component {
 public:
  BME280OneWireSensor() {}

  Sensor* new_temperature_sensor() {
    return &temperature_sensor_;
  }

  Sensor* new_humidity_sensor() {
    return &humidity_sensor_;
  }

  Sensor* new_pressure_sensor() {
    return &pressure_sensor_;
  }

  void set_one_wire(OneWire* one_wire) {
    one_wire_ = one_wire;
  }

  void set_device_address(uint8_t* device_address) {
    device_address_ = device_address;
  }

  void set_update_interval(float update_interval) {
    update_interval_ = update_interval;
  }

  void setup() override {
    temperature_sensor_.set_name("temperature");
    temperature_sensor_.set_unit_of_measurement("°C");

    humidity_sensor_.set_name("humidity");
    humidity_sensor_.set_unit_of_measurement("%");

    pressure_sensor_.set_name("pressure");
    pressure_sensor_.set_unit_of_measurement("hPa");

    bme280_onewire_ = new BME280OneWire(one_wire_, device_address_, update_interval_);

    add_child(bme280_onewire_);
    add_child(&temperature_sensor_);
    add_child(&humidity_sensor_);
    add_child(&pressure_sensor_);
  }

 private:
  OneWire* one_wire_;
  uint8_t* device_address_;
  float update_interval_;

  BME280OneWire* bme280_onewire_;
  Sensor temperature_sensor_;
  Sensor humidity_sensor_;
  Sensor pressure_sensor_;
};

class BME280OneWireSensorDevice : public Component, public SensorDevice {
 public:
  BME280OneWireSensorDevice
