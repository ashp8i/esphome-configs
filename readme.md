# ESPHome [![Discord Chat](https://img.shields.io/discord/429907082951524364.svg)](https://discord.gg/KhAMKrd) [![GitHub release](https://img.shields.io/github/release/esphome/esphome.svg)](https://GitHub.com/esphome/esphome/releases/)

[![ESPHome Logo](https://esphome.io/_images/logo-text.png)](https://esphome.io/)

# esphome-configs

A collection of my personal [ESPHome](https://esphome.io) configs and packages.

## Config Groups

- [Common](#common)
- [Device Specific Config](#device-specific-config)
- [MQTT](#mqtt)
- [MQTT Automation](#mqtt-automation)
- [Sensor](#sensor)
- [Text Sensor](#text-sensor)
- [Switch](#switch)
- [Binary Sensor](#binary-sensor)

## Device Groups

- [Athom 5W GU10](#athom-5w-gu10)
- [MiBoxer WB5](#miboxer-wb5)
- [Lusunt 36W Ceiling Light](#lusunt-36w-ceiling-light)
- [OffDarks 68W Ceiling Light](#offdarks-68w-ceiling-light)
- [Offdarks 28W Ceiling Light](#offdarks-28w-ceiling-light)
- [Lumary 18W Recessed Panel Light](#lumary-18w-recessed-panel-light)
- [Iralan 42W Ceiling Light ESP32-C3](#iralan-42w-ceiling-light-esp32-c3)
- [Iralan 42W Ceiling Light ESP32-S2](#iralan-42w-ceiling-light-esp32-s2)
- [MiBoxer FUT035W](#miboxer-fut035w)
- [MiBoxer FUT039W](#miboxer-fut039w)
- [Shelly 1](#shelly-1)
- [Shelly 2.5](#shelly-25)
- [Shelly Plus i4](#shelly-plus-i4)

# Common

`.base-debug.yaml`

This is the common config is present in all configs. It includes:

- Logger
- OTA with password
- Secret-based Wifi configuration
- Fallback AP with secret password

# Device Specific Config

`.base.devicetype-\*.yaml`

This is a device specific config that will compile correctly for the esp chip used with that device, includes the following:

- ESPHome Template, name, comment platform, board project name and version\*, any platformio options including any special platform packages and framework version and platform version
- Captive Portal
- Web Server for Web API
- Globals required by the device
- Output pins defined
- Components like Tuya, Light, WLED, E1.31
- External Components like PR for ledc when using ESP32-C3 with arduino framework
- UART pins if needed by TuyaMCU
- i2c needed by some Shelly devices
- device specific switches\*\*

\*this will vary by device and helps to show correct data in Home Assistant and ensure firmware compiles correctly. Name, Comment, Project Name and Project Version values are definable in substitutions.

\*\*be sure not to include .base.switch\*.yaml files as esphome will complain duplicate switch: has been defined.

# MQTT

`.base.mqtt-common.yaml`

This is the mqtt broker connection config is present in most configs. It includes:

- MQTT Broker Hostname/IP in `secrets.yaml`
- MQTT Username in `secrets.yaml`
- MQTT Password in `secrets.yaml`
- Discovery to allow Home Assistant to setup device\*\*\*

\*\*\*MQTT Discovery helps to ensure messages are picked up correctly but results in duplicate devices, I have kept this enabled but manually disable the device in Home Assistant cleaning up and double numbering ensuring the MQTT device has \*_2 entity id and has been disabled

# MQTT Automation

.base.mqtt-\*-automation-\*.yaml

This is a fallback mechanism in case Home Assistant becomes Offline. It mainly caters for Shelly Plus i4 and includes:

- Short Click
- Hold
- Double Click

Supports 4 buttons and multiple devices depending on .base.mqtt-\*-automation-\*.yaml file chosen

Device topics are templated in substitutions

# Sensor

`.base.sensor-common.yaml`

Sensors in Home Assistant. It includes:

- WiFi Signal
- Uptime

`.base.sensor-common-die-temp.yaml`

Includes the above plus:

- ESP32 Variants CPU Temperature

# Text Sensor

`.base.text-sensor-common.yaml`

This is the text sensor common config is present in all configs. It includes:

- IP
- SSID
- BSSID
- Human Readable Uptime

# Switch

`.base.switch-common.yaml`

This is the common switch config is present in all configs. It includes:

- Restart Switch
- Restart Switch (Safe Mode)

# Binary Sensor

`.base.binary-sensor-common.yaml`

This is the binary sensor common config is present in all configs. It includes:

- Status

## Setup

1. Copy the required .\*.yaml into your local ESPHome directory.
1. Open `secrets.yaml` and enter your Wifi connection info.
1. Ensure 

# Athom 5W GU10

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

these bulbs come with either tasmota or esphome installed

device comes from factory setup with initial setup Captive Portal and an ESPHome adopt url setup in the default config

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

optional\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if controlled by a Shelly Plus i4

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and it is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP

# MiBoxer WB5

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

Optional
.base.mqtt-light-automation-4-button-rgbcct.yaml - if contreolled by a Shelly Plus i4

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP

# Lusunt 36W Ceiling Light

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP 

# OffDarks 68W Ceiling Light

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP 

# Lumary 18W Recessed Panel Light

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP 

# Iralan 42W Ceiling Light ESP32-C3

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP 

# Iralan 42W Ceiling Light ESP32-S2

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP 

# MiBoxer FUT035W

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP 

# MiBoxer FUT039W

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP 

# Offdarks 28W Ceiling Light

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP 

# Shelly 1

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP 

# Shelly 2.5

`.base.bulb.athom-gu10.yaml`

[![Athom GU10 RGBCW for ESPHome](./.images/ATHOM-Pre-Flashed-ESPHome-Smart-Bulb-ESP8285-Works-With-Home-Assistant-GU10.jpeg)](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)<br />

[Athom GU10 RGBCW for ESPHome](https://www.athom.tech/blank-1/esphome-gu10-rgbcw)\
[AliExpress](https://www.aliexpress.com/item/1005003124769590.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Athom-GU10-Bulb)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP

# Shelly Plus i4

`.base.switch.shelly-plus-i4-arduino.yaml`

[![Shelly Plus i4](./.images/i4-device.png)](https://www.shelly.cloud/shelly-plus-i4)<br />
[![Shelly Plus i4](./.images/plus-series-http.png)](https://www.shelly.cloud/shelly-plus-i4)<br />

[Shelly Plus i4](https://www.shelly.cloud/shelly-plus-i4)\
[Shelly Store Europe](https://shop.shelly.cloud/shelly-plus-i4-wifi-smart-home-automation)\
[Shelly Store UK](https://shellystore.co.uk/product/shelly-plus-i4/)\
[AliExpress](https://www.aliexpress.com/item/1005003774487679.html)\
[ESPHome Devices Page](https://www.esphome-devices.com/devices/Shelly-Plus-i4)

the following files are required and all following substitutions should be set: - name, ssid, description, friendly name, projectname and project version
see bulb.ashish-spot1.yaml for example

`.base-debug.yaml`\
`.base.bulb.athom-gu10.yaml`\
`.base.mqtt-light-automation-4-button-rgbcct.yaml` - if contreolled by a Shelly Plus i4\
`.base.sensor-common.yaml`\
`.base.text-sensor-common.yaml`\
`.base.switch-common.yaml`\
`.base.binary-sensor-common.yaml`

I have split the CT & RGB Channels to avoid overloading the bulb however that should not be an issue, and is a personal preference

This device incorporates the following features in addition to MQTT, MQTT Automation, Sensors, Text Sensors, Restart Switches & Binary Status Sensor:

- ESP8285 based 2MB Flash
- Captive Portal
- Web Server
- WLED
- E1.31
- DDP

# Setup

1. Copy the [common config](../common) into your local ESPHome configurations.
1. Copy `` and `` into your local ESPHome configurations.

## Misc - Template

### Commands

comment reference[phord/Jarvis](https://github.com/phord/Jarvis). I am still testing out some of these commands. Namely:

- Setting units (in, cm)

I have implemented a "go to height" action locally.

### Responses

- Sending a sync command makes the desk send its current

<details>
  <summary>Click to view observed prefix values</summary>

| Height | Response   |
|--------|------------|
| 25.3   | `20, 5`    |
| 25.3   | `20, 8`    |

</details>

## Sources

### Hardware

1.  ["2ANKDJCP35NBLT Bluetooth Box by ZHEJIANG JIECANG LINEAR MOTION TECHNOLOGY CO., LTD"](https://fccid.io/2ANKDJCP35NBLT). (2018, January 25). FCC ID. Retrieved January 19, 2021.
2.  [Jiecang Bluetooth Dongle Product Listing](https://en.jiecang.com/product/131.html). Retrieved January 19, 2021.

#### Images from deadman96385

1.  <https://imgur.com/a/MUbXwnM>
2.  <https://i.imgur.com/DyMf3Ee.jpg>
3.  <https://i.imgur.com/KtsWpVQ.jpg>
4.  <https://i.imgur.com/BS62C1E.jpg>
5.  <https://i.imgur.com/woWoQMe.jpg>
6.  <https://i.imgur.com/Lta5Nab.jpg>

### Software

1.  Justintout. (2020, April 16). GitHub - ["justintout/uplift-reconnect: A Flutter app to control Uplift desks with Uplift Connect BLE modules installed"](https://github.com/justintout/uplift-reconnect). GitHub. Retrieved January 19, 2021.
2.  Deadman96385. (2020, March 6). ["uplift_desk_controller_app/BluetoothHandler.java at a58bcadfb77ac993751758465f1cf20f71d6d8fd - deadman96385/uplift_desk_controller_app"](https://github.com/deadman96385/uplift_desk_controller_app/blob/a58bcadfb77ac993751758465f1cf20f71d6d8fd/app/src/main/java/com/deadman/uplift/BluetoothHandler.java). GitHub. Retrieved January 23, 2021.
3.  Phord. (2021, August 12). ["phord/Jarvis: Hacking the Jarvis standup desk from fully.com for home automation using an ESP8266 arduino interface"](https://github.com/phord/Jarvis). GitHub. Retrieved December 5, 2021.
4.  Ramot, Y. (2015, February 4). ["UpLift Desk wifi link"](https://hackaday.io/project/4173-uplift-desk-wifi-link). Hackaday.io.
5.  Horacek, L. (2019, April 14). ["Standing desk remote control"](https://hackaday.io/project/164931-standing-desk-remote-control). Hackaday.io.
6.  Hunleth, F. (2019, January 18). ["Nerves At Home: Controlling a Desk"](https://embedded-elixir.com/post/2019-01-18-nerves-at-home-desk-controller/). Embedded Elixir. Retrieved January 2021.