import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ['@ashp8i']
MULTI_CONF = True

ow_bus_ng_ns = cg.esphome_ns.namespace('ow_bus_ng')
ESPHomeOneWireNGComponent = ow_bus_ng_ns.class_("ESPHomeOneWireNGComponent", cg.Component)

OneWirePinConfig = cv.enum({
    'single': 'PIN_SINGLE',
    'split_io': 'PIN_SPLIT',
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ESPHomeOneWireNGComponent),
    cv.Optional(CONF_PIN): pins.gpio_output_pin_schema,
    cv.Required('pin_option'): OneWirePinConfig,
    cv.Optional('input_pin'): pins.gpio_input_pin_schema,
    cv.Optional('output_pin'): pins.gpio_output_pin_schema,
}) 

def validate_pin_option(config):
    if config['pin_option'] == 'single':
        if CONF_PIN not in config:
            raise cv.Invalid("When using 'single', the 'pin' must be provided")
    elif config['pin_option'] == 'split_io':
        if 'input_pin' not in config or 'output_pin' not in config:
            raise cv.Invalid("When using 'split_io', both 'input_pin' and 'output_pin' must be provided")
    return config

CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, validate_pin_option)  

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], ow_bus_ng_ns.ESPHomeOneWireNGComponent)
    pin_type = config['pin_option']
    cg.add(var.set_pin_type(pin_type))
    if pin_type == 'single':
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        cg.add(var.set_pin(pin))
    elif pin_type == 'split_io':
        in_pin = await cg.gpio_pin_expression(config['input_pin'])
        out_pin = await cg.gpio_pin_expression(config['output_pin'])
        cg.add(var.set_in_pin(in_pin))
        cg.add(var.set_out_pin(out_pin))
    await cg.register_component(var, config)