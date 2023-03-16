import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN_A, CONF_PIN_B

MULTI_CONF = True
AUTO_LOAD = ["sensor"]

CONF_ONE_WIRE_ID = 'one_wire_id'
dallas_ns = cg.esphome_ns.namespace("shelly_dallas_new")
DallasComponent = dallas_ns.class_("ShellyDallasComponentnew", cg.PollingComponent)
ESPOneWire = shelly_dallas_ns.class_("ESPOneWire")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ShellyDallasComponentnew),
        cv.GenerateID(CONF_ONE_WIRE_ID): cv.declare_id(ESPOneWire),
        cv.Required(CONF_PIN_A): pins.gpio_input_pin_schema,
        cv.Required(CONF_PIN_B): pins.gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], one_wire)
    await cg.register_component(var, config)

    in_pin = await cg.gpio_pin_expression(config[CONF_PIN_A])
    out_pin = await cg.gpio_pin_expression(config[CONF_PIN_B])
    one_wire = cg.new_Pvariable(config[CONF_ONE_WIRE_ID], in_pin, out_pin)
    cg.add(var.set_in_pin(pin))
    cg.add(var.set_out_pin(pin))
