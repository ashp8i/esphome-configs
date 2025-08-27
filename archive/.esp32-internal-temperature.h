// Official docs / ref (C3 specific, but same ref is available for other supported variants):
// https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32c3/api-reference/peripherals/temp_sensor.html

// Provided example:
// https://github.com/espressif/esp-idf/blob/v4.4.2/examples/peripherals/temp_sensor/main/temp_sensor_main.c

// API Implementation (C3 specific, but same ref is available for other supported variants):
// https://github.com/espressif/esp-idf/blob/release/v4.4/components/driver/esp32c3/include/driver/temp_sensor.h

// Note there is an upcoming API change for v5.
  
#include <driver/temp_sensor.h>
#include "esphome.h"
class ESP32InternalTemperature : public PollingComponent, public Sensor {
 public:
  ESP32InternalTemperature() : PollingComponent(15000) {}

  void setup() override {
    ESP_LOGD("ESP32InternalTemperature", "Configuring ESP32 to read die temperatures."); 
    ESP_ERROR_CHECK(temp_sensor_set_config(TSENS_CONFIG_DEFAULT()));
    ESP_ERROR_CHECK(temp_sensor_start());
  }

  void update() override {
    float reading;
    ESP_ERROR_CHECK(temp_sensor_read_celsius(&reading));
    publish_state(reading);
  }
};