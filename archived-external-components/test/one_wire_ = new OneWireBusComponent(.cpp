one_wire_ = new OneWireBusComponent(.pin = pin, .in_pin = in_pin, .out_pin = out_pin)
{
    // Check if the pin supports PWM
    if (!pin_->is_pwm()) {
      ESP_LOGE(TAG, "Pin %d does not support PWM", pin_->get_pin());
      return;
    }

    // Check if the PWM channel is available
    int pwm_channel = pin_->get_pwm_channel();

#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32)
    if (ledc_get_pwm_channel_status(pwm_channel) != LEDC_CHANNEL_FREE) {
      ESP_LOGE(TAG, "PWM channel %d is already in use", pwm_channel);
      return;
    }
#elif defined(ESP8266)
    if (analogWrite(pin_->get_pin(), 0) == 0) {
      ESP_LOGE(TAG, "PWM channel %d is already in use", pwm_channel);
      return;
    }
#endif

    // All checks passed, enable PWM output on the pin
    pin_->setup_pwm();
  }

  if (in_pin && out_pin) {
    // Split I/O mode
    in_pin_ = in_pin->to_isr();
    out_pin_ = out_pin->to_isr();
  } else {
    // Single-pin mode
    pin_ = pin_->to_isr();
  }
}