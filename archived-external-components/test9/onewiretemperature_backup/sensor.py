import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor 
from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
    # 'CONF_ONEWIRE_ID',
    CONF_INDEX,
    CONF_RESOLUTION,
    # CONF_TEMPERATURE, 
    DEVICE_CLASS_TEMPERATURE, 
    STATE_CLASS_MEASUREMENT,
    # UNIT_PERCENT
    UNIT_CELSIUS,  
)

from esphome.components.onewirebus import OneWireBus, OneWireBusComponent

# bus = onewirebus_ns.OneWireBusComponent()

DEPENDENCIES = ["onewirebus"]

OneWireTemperature = onewirebus_ns.class_("OneWireTemperature", sensor.Sensor)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        OneWireTemperature,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(
        {
            # cv.GenerateID(CONF_ONEWIRE_ID): cv.use_id(OneWireBusComponent),
            cv.GenerateID(CONF_ID): cv.use_id(OneWireBusComponent),
            cv.Optional(CONF_ADDRESS): cv.hex_int,
            cv.Optional(CONF_INDEX): cv.positive_int,
            cv.Optional(CONF_RESOLUTION, default=12): cv.int_range(min=9, max=12),
        }
    ),
    cv.has_exactly_one_key(CONF_ADDRESS, CONF_INDEX),
)


async def to_code(config):
    # hub = await cg.get_variable(config[CONF_ONEWIRE_ID])
    hub = await cg.get_variable(config[CONF_ID])
    var = await sensor.new_sensor(config)

    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    else:
        cg.add(var.set_index(config[CONF_INDEX]))

    if CONF_RESOLUTION in config:
        cg.add(var.set_resolution(config[CONF_RESOLUTION]))

    cg.add(var.set_parent(hub))

    cg.add(hub.register_sensor(var))

# OneWireDevice = onewirebus_ns.class_("OneWireDevice", onewirebus_ns.OneWireDevice)

# CONFIG_SCHEMA = cv.Schema({
#    cv.Required(CONF_ONEWIRE_ID): cv.use_id(onewirebus_ns.OneWireBusComponent),
#     cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
#         unit_of_measurement=UNIT_CELSIUS,
#         accuracy_decimals=1,
#         device_class=DEVICE_CLASS_TEMPERATURE,
#         state_class=STATE_CLASS_MEASUREMENT
#     ),
# })

# async def to_code(config):
#     parent = await cg.get_variable(config[CONF_ONEWIRE_ID])    
#     var = cg.new_Pvariable(config[CONF_ID], parent)  
#     await cg.register_component(var, config)

#     if CONF_TEMPERATURE in config:
#         sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
#         cg.add(var.set_temperature_sensor(sens))
    
#     cg.add(parent.register_sensor(var))