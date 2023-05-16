import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN

from onewirebus import OneWireBus, OneWireBusComponent  
from esphome import coroutine_with_priority

from . import onewirebus

__all__ = ["onewirebus"]

AUTO_LOAD = ["sensor"] 

MULTI_CONF = True

@coroutine_with_priority(200)   
async def setup(config):    
    assert CONF_PIN in config      
    pin = await cg.gpio_pin_expression(config[CONF_PIN])       
    cg.add(OneWireBusComponent.new(pin))

@coroutine_with_priority(200)
async def to_code(config):   
    pin = await cg.gpio_pin_expression(config[CONF_PIN])   
    cg.add(OneWireBus.new(pin))