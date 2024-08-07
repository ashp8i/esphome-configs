import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

MULTI_CONF = True

onewire_bus_ns = cg.esphome_ns.namespace("onewire_bus")
OneWireBusComponent = onewire_bus_ns.class_("OneWireBusComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OneWireBusComponent),
        cv.Required(CONF_PIN): pins.gpio_input_pin_schema,
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))