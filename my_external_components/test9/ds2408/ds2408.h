class DS2408Device : public OneWireBusDevice {
  public:
    // ...  

    bool get_switch_state(uint8_t channel);
    void set_switch_state(uint8_t channel, bool state);

  private:
    bool read_registers() override;
    bool check_registers() override;
};

bool DS2408Device::get_switch_state(uint8_t channel) {
  // Read scratchpad and parse data to get state of channel
}

void DS2408Device::set_switch_state(uint8_t channel, bool state) {
  // Write command to set state of channel 
}