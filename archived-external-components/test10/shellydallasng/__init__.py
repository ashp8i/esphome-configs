import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID

MULTI_CONF = True
AUTO_LOAD = ["sensor"]

shellydallasng_ns = cg.esphome_ns.namespace("shellydallasng")
ShellyDallasNgComponent = shellydallasng_ns.class_("ShellyDallasNgComponent", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ShellyDallasNgComponent),
        cv.Required('input_pin'): pins.internal_gpio_input_pin_schema,
        cv.Required('output_pin'): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_library("https://github.com/pstolarz/OneWireNg", None)

    input_pin = await cg.gpio_pin_expression(config['input_pin'])
    input_pin_num = input_pin.get_pin()

    output_pin = await cg.gpio_pin_expression(config['output_pin']) 
    output_pin_num = output_pin.get_pin()
    
    cg.add(var.set_pins(input_pin_num, output_pin_num))
