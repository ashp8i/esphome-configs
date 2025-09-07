import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN_A, CONF_PIN_B

MULTI_CONF = True
AUTO_LOAD = ["sensor"]

shelly_dallas_new_ns = cg.esphome_ns.namespace("shelly_dallas_new")
ShellyDallasComponentnew = shelly_dallas_new_ns.class_("ShellyDallasComponentnew", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ShellyDallasComponentnew),
        cv.Required(CONF_PIN_A): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_PIN_B): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    in_pin = await cg.gpio_pin_expression(config[CONF_PIN_A])
    out_pin = await cg.gpio_pin_expression(config[CONF_PIN_B])
    cg.add(var.set_pins(in_pin, out_pin))
