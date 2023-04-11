/*
Copyright (c) 2007, Jim Studt  (original old version - many contributors since)

The latest version of this library may be found at:
  http://www.pjrc.com/teensy/td_libs_OneWire.html

OneWire has been maintained by Paul Stoffregen (paul@pjrc.com) since
January 2010.

DO NOT EMAIL for technical support, especially not for ESP chips!
All project support questions must be posted on public forums
relevant to the board or chips used.  If using Arduino, post on
Arduino's forum.  If using ESP, post on the ESP community forums.
There is ABSOLUTELY NO TECH SUPPORT BY PRIVATE EMAIL!

Github's issue tracker for OneWire should be used only to report
specific bugs.  DO NOT request project support via Github.  All
project and tech support questions must be posted on forums, not
github issues.  If you experience a problem and you are not
absolutely sure it's an issue with the library, ask on a forum
first.  Only use github to report issues after experts have
confirmed the issue is with OneWire rather than your project.

Back in 2010, OneWire was in need of many bug fixes, but had
been abandoned the original author (Jim Studt).  None of the known
contributors were interested in maintaining OneWire.  Paul typically
works on OneWire every 6 to 12 months.  Patches usually wait that
long.  If anyone is interested in more actively maintaining OneWire,
please contact Paul (this is pretty much the only reason to use
private email about OneWire).

OneWire is now very mature code.  No changes other than adding
definitions for newer hardware support are anticipated.

Version 2.3:
  Unknown chip fallback mode, Roger Clark
  Teensy-LC compatibility, Paul Stoffregen
  Search bug fix, Love Nystrom

Version 2.2:
  Teensy 3.0 compatibility, Paul Stoffregen, paul@pjrc.com
  Arduino Due compatibility, http://arduino.cc/forum/index.php?topic=141030
  Fix DS18B20 example negative temperature
  Fix DS18B20 example's low res modes, Ken Butcher
  Improve reset timing, Mark Tillotson
  Add const qualifiers, Bertrik Sikken
  Add initial value input to crc16, Bertrik Sikken
  Add target_search() function, Scott Roberts

Version 2.1:
  Arduino 1.0 compatibility, Paul Stoffregen
  Improve temperature example, Paul Stoffregen
  DS250x_PROM example, Guillermo Lovato
  PIC32 (chipKit) compatibility, Jason Dangel, dangel.jason AT gmail.com
  Improvements from Glenn Trewitt:
  - crc16() now works
  - check_crc16() does all of calculation/checking work.
  - Added read_bytes() and write_bytes(), to reduce tedious loops.
  - Added ds2408 example.
  Delete very old, out-of-date readme file (info is here)

Version 2.0: Modifications by Paul Stoffregen, January 2010:
http://www.pjrc.com/teensy/td_libs_OneWire.html
  Search fix from Robin James
    http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1238032295/27#27
  Use direct optimized I/O in all cases
  Disable interrupts during timing critical sections
    (this solves many random communication errors)
  Disable interrupts during read-modify-write I/O
  Reduce RAM consumption by eliminating unnecessary
    variables and trimming many to 8 bits
  Optimize both crc8 - table version moved to flash

Modified to work with larger numbers of devices - avoids loop.
Tested in Arduino 11 alpha with 12 sensors.
26 Sept 2008 -- Robin James
http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1238032295/27#27

Updated to work with arduino-0008 and to include skip() as of
2007/07/06. --RJL20

Modified to calculate the 8-bit CRC directly, avoiding the need for
the 256-byte lookup table to be loaded in RAM.  Tested in arduino-0010
-- Tom Pollard, Jan 23, 2008

Jim Studt's original library was modified by Josh Larios.

Tom Pollard, pollard@alum.mit.edu, contributed around May 20, 2008

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Much of the code was inspired by Derek Yerger's code, though I don't
think much of that remains.  In any event that was..
    (copyleft) 2006 by Derek Yerger - Free to distribute freely.

The CRC code was excerpted and inspired by the Dallas Semiconductor
sample code bearing this copyright.
---------------------------------------------------------------------------
Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Dallas Semiconductor
shall not be used except as stated in the Dallas Semiconductor
Branding Policy.
--------------------------------------------------------------------------
*/

#include "custom_onewire.h"

namespace esphome {
namespace custom_onewire {

static const char *const TAG = "dallas_maxim.one_wire";

const uint8_t ONE_WIRE_ROM_SELECT = 0x55;
const int ONE_WIRE_ROM_SEARCH = 0xF0;

CustomOneWire::CustomOneWire(InternalGPIOPin *pin, bool low_power_mode, bool overdrive_mode)
    : pin_(pin), rom_number_(0), last_discrepancy_(0), last_device_flag_(false), state_(State::Reset),
      low_power_mode_(low_power_mode), overdrive_mode_(overdrive_mode) { pin_ = pin->to_isr(); }

bool HOT IRAM_ATTR CustomOneWire::reset() {
  // See reset here:
  // https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
  // Wait for communication to clear (delay G)
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  uint8_t retries = 125;
  do {
    if (--retries == 0)
      return false;
    delayMicroseconds(2);
  } while (!pin_.digital_read());

  // Send 480µs LOW TX reset pulse (drive bus low, delay H)
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);
  delayMicroseconds(480);

  // Release the bus, delay I
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  delayMicroseconds(70);

  // sample bus, 0=device(s) present, 1=no device present
  bool r = !pin_.digital_read();
  // delay J
  delayMicroseconds(410);
  return r;
}

void HOT IRAM_ATTR CustomOneWire::write_bit(bool bit) {
  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);

  // from datasheet:
  // write 0 low time: t_low0: min=60µs, max=120µs
  // write 1 low time: t_low1: min=1µs, max=15µs
  // time slot: t_slot: min=60µs, max=120µs
  // recovery time: t_rec: min=1µs
  // ds18b20 appears to read the bus after roughly 14µs
  uint32_t delay0 = bit ? 6 : 60;
  uint32_t delay1 = bit ? 54 : 5;

  // delay A/C
  delayMicroseconds(delay0);
  // release bus
  pin_.digital_write(true);
  // delay B/D
  delayMicroseconds(delay1);
}

bool HOT IRAM_ATTR CustomOneWire::read_bit() {
  // drive bus low
  pin_.pin_mode(gpio::FLAG_OUTPUT);
  pin_.digital_write(false);

  // note: for reading we'll need very accurate timing, as the
  // timing for the digital_read() is tight; according to the datasheet,
  // we should read at the end of 16µs starting from the bus low
  // typically, the ds18b20 pulls the line high after 11µs for a logical 1
  // and 29µs for a logical 0

  uint32_t start = micros();
  // datasheet says >1µs
  delayMicroseconds(3);

  // release bus, delay E
  pin_.pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  // Unfortunately some frameworks have different characteristics than others
  // esp32 arduino appears to pull the bus low only after the digital_write(false),
  // whereas on esp-idf it already happens during the pin_mode(OUTPUT)
  // manually correct for this with these constants.

#ifdef USE_ESP32
  uint32_t timing_constant = 12;
#else
  uint32_t timing_constant = 14;
#endif

  // measure from start value directly, to get best accurate timing no matter
  // how long pin_mode/delayMicroseconds took
  while (micros() - start < timing_constant)
    ;

  // sample bus to read bit from peer
  bool r = pin_.digital_read();

  // read slot is at least 60µs; get as close to 60µs to spend less time with interrupts locked
  uint32_t now = micros();
  if (now - start < 60)
    delayMicroseconds(60 - (now - start));

  return r;
}

void IRAM_ATTR CustomOneWire::write8(uint8_t val) {
  for (uint8_t i = 0; i < 8; i++) {
    this->write_bit(bool((1u << i) & val));
  }
}

void IRAM_ATTR CustomOneWire::write64(uint64_t val) {
  for (uint8_t i = 0; i < 64; i++) {
    this->write_bit(bool((1ULL << i) & val));
  }
}

uint8_t IRAM_ATTR CustomOneWire::read8() {
  uint8_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint8_t(this->read_bit()) << i);
  }
  return ret;
}
uint64_t IRAM_ATTR CustomOneWire::read64() {
  uint64_t ret = 0;
  for (uint8_t i = 0; i < 8; i++) {
    ret |= (uint64_t(this->read_bit()) << i);
  }
  return ret;
}
void IRAM_ATTR CustomOneWire::select(uint64_t address) {
  this->write8(ONE_WIRE_ROM_SELECT);
  this->write64(address);
}
void IRAM_ATTR CustomOneWire::reset_search() {
  this->last_discrepancy_ = 0;
  this->last_device_flag_ = false;
  this->rom_number_ = 0;
}
uint64_t IRAM_ATTR CustomOneWire::search() {
  if (this->last_device_flag_) {
    return 0u;
  }

  {
    InterruptLock lock;
    if (!this->reset()) {
      // Reset failed or no devices present
      this->reset_search();
      return 0u;
    }
  }

  uint8_t id_bit_number = 1;
  uint8_t last_zero = 0;
  uint8_t rom_byte_number = 0;
  bool search_result = false;
  uint8_t rom_byte_mask = 1;

  {
    InterruptLock lock;
    // Initiate search
    this->write8(ONE_WIRE_ROM_SEARCH);
    do {
      // read bit
      bool id_bit = this->read_bit();
      // read its complement
      bool cmp_id_bit = this->read_bit();

      if (id_bit && cmp_id_bit) {
        // No devices participating in search
        break;
      }

      bool branch;

      if (id_bit != cmp_id_bit) {
        // only chose one branch, the other one doesn't have any devices.
        branch = id_bit;
      } else {
        // there are devices with both 0s and 1s at this bit
        if (id_bit_number < this->last_discrepancy_) {
          branch = (this->rom_number8_()[rom_byte_number] & rom_byte_mask) > 0;
        } else {
          branch = id_bit_number == this->last_discrepancy_;
        }

        if (!branch) {
          last_zero = id_bit_number;
        }
      }

      if (branch) {
        // set bit
        this->rom_number8_()[rom_byte_number] |= rom_byte_mask;
      } else {
        // clear bit
        this->rom_number8_()[rom_byte_number] &= ~rom_byte_mask;
      }

      // choose/announce branch
      this->write_bit(branch);
      id_bit_number++;
      rom_byte_mask <<= 1;
      if (rom_byte_mask == 0u) {
        // go to next byte
        rom_byte_number++;
        rom_byte_mask = 1;
      }
    } while (rom_byte_number < 8);  // loop through all bytes
  }

  if (id_bit_number >= 65) {
    this->last_discrepancy_ = last_zero;
    if (this->last_discrepancy_ == 0) {
      // we're at root and have no choices left, so this was the last one.
      this->last_device_flag_ = true;
    }
    search_result = true;
  }

  search_result = search_result && (this->rom_number8_()[0] != 0);
  if (!search_result) {
    this->reset_search();
    return 0u;
  }

  return this->rom_number_;
}
std::vector<uint64_t> CustomOneWire::search_vec() {
  std::vector<uint64_t> res;

  this->reset_search();
  uint64_t address;
  while ((address = this->search()) != 0u)
    res.push_back(address);

  return res;
}
void IRAM_ATTR CustomOneWire::skip() {
  this->write8(0xCC);  // skip ROM
}

uint8_t IRAM_ATTR *CustomOneWire::rom_number8_() { return reinterpret_cast<uint8_t *>(&this->rom_number_); }

void IRAM_ATTR CustomOneWire::set_low_power() {
  // reduce bus voltage to save power
  pin_.analog_write(0);
}

void IRAM_ATTR CustomOneWire::set_overdrive() {
  // reduce delay times for overdrive mode
  uint32_t delay0 = bit ? 1 : 5;
  uint32_t delay1 = bit ? 4 : 1;
}

}  // namespace custom_onewire
}  // namespace esphome

