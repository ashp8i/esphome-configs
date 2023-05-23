import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

MULTI_CONF = True
AUTO_LOAD = ["sensor"]

onewire_ns = cg.esphome_ns.namespace("onewire")
OneWireComponent = onewire_ns.class_("OneWireComponent", cg.PollingComponent)

COMPONENT_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(OneWireComponent),
}).extend(cv.COMPONENT_SCHEMA)

PIN_SCHEMA = cv.Schema({  
    cv.Required(CONF_ONEWIRE_BUS_GPIO): cv.int_,     
    cv.Optional(CONF_PIN): cv.gpio_output_pin_schema,   
    cv.Optional(CONF_INPUT): cv.gpio_input_pin_schema,
    cv.Optional(CONF_OUTPUT): cv.gpio_output_pin_schema
})

CONFIG_SCHEMA = PIN_SCHEMA.extend(COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])  
    await cg.register_component(var, config)  
    
    onewire_gpio = config[CONF_ONEWIRE_BUS_GPIO]
    
    if CONF_PIN in config:
        pin = await cg.gpio_pin(config[CONF_PIN])
        cg.add(var.set_pin(pin)) 
    elif CONF_INPUT in config and CONF_OUTPUT in config:
       input_pin = await cg.gpio_pin(config[CONF_INPUT])  
       output_pin = await cg.gpio_pin(config[CONF_OUTPUT])
