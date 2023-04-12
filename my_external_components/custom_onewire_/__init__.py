import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

MULTI_CONF = True

custom_onewire_ns = cg.esphome_ns.namespace("custom_onewire")
CustomOneWire = custom_onewire_ns.class_("CustomOneWire", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomOneWire),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    }).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_library("https://github.com/ashp8i/custom_onewire", None)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))