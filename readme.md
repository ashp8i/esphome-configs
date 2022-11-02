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

.base-debug.yaml

This is the common config is present in all configs. It includes:

- Logger
- OTA with password
- Secret-based Wifi configuration
- Fallback AP with secret password

# Device Specific Config

.base.devicetype-\*.yaml

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

.base.mqtt-common.yaml

This is the mqtt broker connection config is present in most configs. It includes:

- Broker Hostname/IP in secrets.yaml
- Broker Hostname/IP in secrets.yaml
- Broker Hostname/IP in secrets.yaml
- Discovery to allow Home Assistant to setup device\*\*\*

\*\*\*MQTT Discovery helps to ensure messages are picked up correctly but results in duplicate devices, I have kept this enabled but manually disable the device in Home Assistant cleaning up and double numbering ensuring the MQTT device has \*_2 entity id and has been disabled

# MQTT Automation

.base.mqtt-\*-automation-\*.yaml

This is a fallback mechanism in case Home Assistant becomes Offline. It mainly caters for Shelly Plus i4 includes:

- Short Click
- Hold
- Double Click

Supports 4 buttons and multiple devices depending on .base.mqtt-\*-automation-\*.yaml file chosen

Device topics are templated in substitutions

# Sensor

.base.sensor-common.yaml

Sensors in Home Assistant. It includes:

- WiFi Signal
- Uptime

.base.sensor-common-die-temp.yaml

Includes the above plus:

- ESP32 Variants CPU Temperature

# Text Sensor

.base.text-sensor-common.yaml

This is the text sensor common config is present in all configs. It includes:

- IP
- SSID
- BSSID
- Human Readable Uptime

# Switch

.base.switch-common.yaml

This is the common switch config is present in all configs. It includes:

- Restart Switch
- Restart Switch (Safe Mode)

# Binary Sensor

.base.binary-sensor-common.yaml

This is the binary common config is present in all configs. It includes:

- Status

## Setup

1. Copy the required .\*.yaml into your local ESPHome directory.
1. Open `secrets.yaml` and enter your Wifi connection info.

# Athom 5W GU10

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# MiBoxer WB5

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# Lusunt 36W Ceiling Light

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# OffDarks 68W Ceiling Light

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# Lumary 18W Recessed Panel Light

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# Iralan 42W Ceiling Light ESP32-C3

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# Iralan 42W Ceiling Light ESP32-S2

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# MiBoxer FUT035W

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# MiBoxer FUT039W

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# Offdarks 28W Ceiling Light

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# Shelly 1

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# Shelly 2.5

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed 

# Shelly Plus i4

.base.bulb.athom-gu10.yaml

these bulbs come with either tasmota or esphome installed

# Setup

1. Copy the [common config](../common) into your local ESPHome configurations.
1. Copy `light_strip.yaml` and `packages/nitebird.yaml` into your local ESPHome configurations.

# Uplift Desk

<img src="../../assets/uplift_desk/home-assistant.png?raw=true" width="50%">

A configuration for [Uplift Desk](https://www.upliftdesk.com/) dongle uses.

## Supported Features

Currently, this integration supports the following:

- Commands
  - Move up
- Sensors
  - Current height reading

## RJ12 Pinout

<img src="../../assets/uplift_desk/desk-pinout.jpg?raw=true" style="width: 25%">

I found the Uplift Connect dongle pinout via the [FCC filing](https://fccid.io/2ANKDJCP35NBLT/Internal-Photos/Internal-Photos-3727739) (Thanks to deadman96385 for sending that my way)

There are 6 PINs follows:

1. ? (Not known but not necessary)
1. GND

## Hardware Setup

<img src="../../assets/uplift_desk/esp-wiring.jpg?raw=true" style="width: 25%">

My coment.

## Software Setup

1. Copy the [common config](../common) into your local ESPHome configurations.
1. Copy `uplift_desk.yaml` and `packages/uplift_desk.yaml` into your local ESPHome configurations.

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