import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

MULTI_CONF = True
AUTO_LOAD = ["sensor"]

shelly_dallas_new_ns = cg.esphome_ns.namespace("shelly_dallas_new")
ShellyDallasComponentnew = shelly_dallas_new_ns.class_("ShellyDallasComponentnew", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ShellyDallasComponentnew),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
