import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

CONF_INPUT_PIN = "input_pin"
CONF_OUTPUT_PIN = "output_pin"

MULTI_CONF = True
AUTO_LOAD = ["sensor"]

onewirebus_ns = cg.esphome_ns.namespace("onewirebus")
OneWireBusComponent = onewirebus_ns.class_("OneWireBusComponent", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OneWireBusComponent),
        cv.Optional(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_INPUT_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_OUTPUT_PIN): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_PIN in config: 
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        cg.add(var.set_pin(pin))
    elif CONF_INPUT_PIN in config and CONF_OUTPUT_PIN in config:
        input_pin = await cg.gpio_pin_expression(config['input_pin'])
        output_pin = await cg.gpio_pin_expression(config['output_pin'])
        cg.add(var.set_pins(input_pin, output_pin))
