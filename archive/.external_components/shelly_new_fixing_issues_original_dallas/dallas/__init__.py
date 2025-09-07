import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN, CONF_PINS
from esphome.config_validation import All

CONF_INPUT_PIN = "input_pin"
CONF_OUTPUT_PIN = "output_pin"

MULTI_CONF = True
AUTO_LOAD = ["sensor"]

dallas_ns = cg.esphome_ns.namespace("dallas")
DallasComponent = dallas_ns.class_("DallasComponent", cg.PollingComponent)

def validate_pin_configuration(config):
    has_input_pin = CONF_INPUT_PIN in config
    has_output_pin = CONF_OUTPUT_PIN in config
    has_pin = CONF_PIN in config

    if has_input_pin and has_output_pin and has_pin:
        raise cv.Invalid("Cannot have 'input_pin', 'output_pin', and 'pin' at the same time.")
    elif has_input_pin and not has_output_pin:
        raise cv.Invalid("If 'input_pin' is provided, 'output_pin' must also be provided.")
    elif has_output_pin and not has_input_pin:
        raise cv.Invalid("If 'output_pin' is provided, 'input_pin' must also be provided.")
    elif not has_pin and not (has_input_pin and has_output_pin):
        raise cv.Invalid("Either 'pin' or both 'input_pin' and 'output_pin' must be provided.")
    return config

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DallasComponent),
        cv.Optional(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_INPUT_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_OUTPUT_PIN): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("60s"))

CONFIG_SCHEMA = All(CONFIG_SCHEMA, validate_pin_configuration)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        cg.add(var.set_pin(pin))
		
    elif CONF_INPUT_PIN in config and CONF_OUTPUT_PIN in config:
        input_pin = await cg.gpio_pin_expression(config[CONF_INPUT_PIN])
        output_pin = await cg.gpio_pin_expression(config[CONF_OUTPUT_PIN])
        pins = [input_pin, output_pin]
        cg.add(var.set_pins(pins))
