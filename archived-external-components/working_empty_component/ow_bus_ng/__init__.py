import voluptuous as vol
import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN
from voluptuous import Required, Optional, Any

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@ashp8i"]
MULTI_CONF = True

ow_bus_ng_ns = cg.esphome_ns.namespace("ow_bus_ng")
ESPHomeOneWireNGComponent = ow_bus_ng_ns.class_(
    "ESPHomeOneWireNGComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESPHomeOneWireNGComponent),
        cv.Optional("pin"): pins.gpio_input_pin_schema,
        cv.Optional("input_pin"): pins.gpio_input_pin_schema,
        cv.Optional("output_pin"): pins.gpio_output_pin_schema,
    }  
)

async def to_code(config):
  var = cg.new_Pvariable(config[CONF_ID], ESPHomeOneWireNGComponent())
  
  if "input_pin" in config: 
    in_pin = await cg.gpio_pin_expression(config["input_pin"])
  if "output_pin" in config:
    out_pin = await cg.gpio_pin_expression(config["output_pin"])
  if "input_pin" in config and "output_pin" in config:
    cg.add(var.set_split_io(in_pin, out_pin))
  else:  
    # Single pin mode
    pin = await cg.gpio_pin_expression(config["pin"])
    cg.add(var.set_single_pin(pin))  

  await cg.register_component(var, config)
