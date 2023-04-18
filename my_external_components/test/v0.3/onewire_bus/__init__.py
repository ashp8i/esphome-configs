
import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    # CONF_ID,
    CONF_PIN,
    CONF_MODE,
    CONF_NAME,
)

_LOGGER = logging.getLogger(__name__)
CONF_ONE_WIRE_BUS_ID = 'onewire_bus_id'

onewire_bus_ns = cg.esphome_ns.namespace("onewire_bus")
OneWireBusComponent = onewire_bus_ns.class_("OneWireBusComponent", cg.Component)

ONEWIRE_BUS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ONE_WIRE_BUS_ID): cv.declare_id(OneWireBusComponent),
        cv.Optional(CONF_PIN): pins.gpio_input_pin_schema,
        cv.Optional('in_pin'): pins.gpio_input_pin_schema,
        cv.Optional('out_pin'): pins.gpio_output_pin_schema,
        cv.Optional('low_power_mode', default=False): cv.boolean,
        cv.Optional('overdrive_mode', default=False): cv.boolean,
        cv.Optional('parasitic_power_mode', default=False): cv.boolean,
        cv.Optional(CONF_NAME): cv.string,
        cv.Required(CONF_MODE): cv.enum({
            'normal': OneWireBusComponent.NORMAL,
            'low_power': OneWireBusComponent.LOW_POWER,
            'overdrive': OneWireBusComponent.OVERDRIVE,
        }, lower=True),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ONE_WIRE_BUS_ID): cv.declare_id(OneWireBusComponent),
        cv.Optional(CONF_PIN): pins.gpio_input_pin_schema,
        cv.Optional('in_pin'): pins.gpio_input_pin_schema,
        cv.Optional('out_pin'): pins.gpio_output_pin_schema,
        cv.Optional('low_power_mode', default=False): cv.boolean,
        cv.Optional('overdrive_mode', default=False): cv.boolean,
        cv.Optional('parasitic_power_mode', default=False): cv.boolean,
        cv.Optional(CONF_NAME): cv.string,
        cv.Required(CONF_MODE): cv.enum({
            'normal': OneWireBusComponent.NORMAL,
            'low_power': OneWireBusComponent.LOW_POWER,
            'overdrive': OneWireBusComponent.OVERDRIVE,
        }, lower=True),
    }
)

def to_code(config):
    pin = None
    if CONF_PIN in config:
        pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    elif 'in_pin' in config:
        pin = yield cg.gpio_pin_expression(config['in_pin'])
    out_pin = None
    if 'out_pin' in config:
        out_pin = yield cg.gpio_pin_expression(config['out_pin'])

    mode_str = config[CONF_MODE]
    mode = OneWireBusComponent[mode_str.upper()]

    # Use get() method to get the name, and provide a default value of None
    name = config.get(CONF_NAME, None)

    var = cg.new_Pvariable(config[CONF_ONE_WIRE_BUS_ID], pin, out_pin, mode)

    if 'low_power_mode' in config:
        cg.add(var.set_low_power_mode(config['low_power_mode']))
    if 'overdrive_mode' in config:
        cg.add(var.set_overdrive_mode(config['overdrive_mode']))
    if 'parasitic_power_mode' in config:
        cg.add(var.set_parasitic_power_mode(config['parasitic_power_mode']))
    yield var