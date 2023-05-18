import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

MULTI_CONF = True
AUTO_LOAD = ["sensor"]

shellydallasng_ns = cg.esphome_ns.namespace("shellydallasng")
ShellyDallasNgComponent = shellydallasng_ns.class_("ShellyDallasNgComponent", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ShellyDallasNgComponent),
        cv.Optional(CONF_PIN): pins.internal_gpio_pin_schema,
        cv.Optional('input_pin'): pins.internal_gpio_input_pin_schema,
        cv.Optional('output_pin'): pins.internal_gpio_output_pin_schema 
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_library("https://github.com/pstolarz/OneWireNg", None)

    if 'pin' in config: 
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        cg.add(var.set_pin(pin))
    else if 'input_pin' in config and 'output_pin' in config: 
        input_pin = await cg.gpio_pin_expression(config['input_pin'])
        output_pin = await cg.gpio_pin_expression(config['output_pin'])
        cg.add(var.set_pins(input_pin, output_pin))
