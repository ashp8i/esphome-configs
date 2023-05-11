#include "onewirebus.h"
#include "gtest/gtest.h"

// Mock pin class for testing
class MockPin {
  public:
    MockPin() : val_(true) {}
    void pin_mode(gpio::Flags) {}
    bool digital_read() { return val_; }
    void digital_write(bool val) { val_ = val; } 

  bool val_;
};

TEST(OneWireBusTest, SearchFindsOneDevice) {
  MockPin pin;
  OneWireBus bus(&pin);

  // Set up search to find 0b01010101 at bit index 40
  for (int i=0; i<7; i++) {
    pin.val_ = true; // 1 bit
    bus.read_bit();
    pin.val_ = false; // 0 bit
    bus.read_bit();
  }
  pin.val_ = true; // 1 bit at index 40
  bus.read_bit();

  auto addr = bus.search();
  ASSERT_EQ(addr, 0x0101010101010101ULL);
}

TEST(OneWireBusTest, SearchFindsTwoDevices) {
  MockPin pin;
  OneWireBus bus(&pin);

  // Set up search to find:
  // 0b01010101 at bit index 40
  // 0b10101010 at bit index 50
  for (int i=0; i<7; i++) {
    pin.val_ = true;  
    bus.read_bit();
    pin.val_ = false; 
    bus.read_bit();
  }
  pin.val_ = true;   // 1 bit at index 40
  bus.read_bit();

  pin.val_ = false; // 0 bit at index 50
  bus.read_bit();
  pin.val_ = true;  // 1 bit at index 50
  bus.read_bit();
  
  auto addr1 = bus.search();
  auto addr2 = bus.search();
  ASSERT_EQ(addr1, 0x0101010101010101ULL);
  ASSERT_EQ(addr2, 0x1010101010101010ULL);
}