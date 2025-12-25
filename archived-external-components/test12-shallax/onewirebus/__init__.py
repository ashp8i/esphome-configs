import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_OUTPUT,
    CONF_SCAN,
    CONF_ADDRESS,
    # CONF_ONEWIRE_ID,
)
from esphome.core import coroutine_with_priority, CORE

CODEOWNERS = ["@esphome/core"]
onewire_ns = cg.esphome_ns.namespace("OneWire")
OneWireBus = onewire_ns.class_("OneWireBus", cg.Component)
OneWireDevice = onewire_ns.class_("OneWireDevice")
ONEWIRE_SPEED = 'onewire_speed'
CONF_ONEWIRE_ADDRESS = 'onewire_address'

# CONF_SDA_PULLUP_ENABLED = "sda_pullup_enabled"
# CONF_SCL_PULLUP_ENABLED = "scl_pullup_enabled"
MULTI_CONF = True

onewire_speed = cv.one_of("normal", "overdrive")

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

pin_with_input_and_output_support = cv.All(
    pins.internal_gpio_pin_number({CONF_INPUT: True}),
    pins.internal_gpio_pin_number({CONF_OUTPUT: True}),
)


def onewire_id_pin_schema(default_speed="slow"):
    schema = {
        cv.GenerateID(CONF_ONEWIRE_ID): cv.use_id(OneWireBus),
        cv.Optional(CONF_ONEWIRE_SPEED, default=default_speed): onewire_speed
    }
    return cv.Schema(schema)

CONFIG_SCHEMA = cv.maybe_simple_id({
    cv.GenerateID(): cv.declare_id(OneWireBus),
    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema, 
    cv.Optional(CONF_ONEWIRE_SPEED, default="slow"):onewire_speed, 
}).extend(cv.polling_component_schema("60s"))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_speed(config[CONF_ONEWIRE_SPEED]))

@coroutine_with_priority(1.0)
async def register_onewire_device(var, config):
    parent = await cg.get_variable(config[CONF_ONEWIRE_ID])
    cg.add(var.set_onewire_bus(parent))
    cg.add(var.set_onewire_address(config[CONF_ONEWIRE_ADDRESS])) 

def final_validate_onewire_device_schema(name):
    schema = cv.Schema(
        {
            cv.Required(CONF_ONEWIRE_ID): fv.id_declaration_match_schema(), 
            cv.Optional(CONF_ONEWIRE_INDEX): cv.any_int, 
            cv.Optional(CONF_ONEWIRE_ADDRESS): cv.any_string
        }, 
        extra=cv.ALLOW_EXTRA
    )
    def validate(config):
        if CONF_ONEWIRE_INDEX not in config and CONF_ONEWIRE_ADDRESS not in config:
            raise cv.Invalid("Must specify either 'onewire_index' or 'onewire_address'")
    schema = schema.validate_with(validate)
    return schema  

def final_validate_onewire_device_schema(name):
    return cv.Schema(
        {cv.Required(CONF_ONEWIRE_ID): fv.id_declaration_match_schema()},
        extra=cv.ALLOW_EXTRA,
    )  


@coroutine_with_priority(1.0)
async def to_code(config):
    cg.add_global(i2c_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_sda_pin(config[CONF_SDA]))
    if CONF_SDA_PULLUP_ENABLED in config:
        cg.add(var.set_sda_pullup_enabled(config[CONF_SDA_PULLUP_ENABLED]))
    cg.add(var.set_scl_pin(config[CONF_SCL]))
    if CONF_SCL_PULLUP_ENABLED in config:
        cg.add(var.set_scl_pullup_enabled(config[CONF_SCL_PULLUP_ENABLED]))

    cg.add(var.set_frequency(int(config[CONF_FREQUENCY])))
    cg.add(var.set_scan(config[CONF_SCAN]))
    if CORE.using_arduino:
        cg.add_library("Wire", None)


def i2c_device_schema(default_address):
    """Create a schema for a i2c device.

    :param default_address: The default address of the i2c device, can be None to represent
      a required option.
    :return: The i2c device schema, `extend` this in your config schema.
    """
    schema = {
        cv.GenerateID(CONF_I2C_ID): cv.use_id(I2CBus),
        cv.Optional("multiplexer"): cv.invalid(
            "This option has been removed, please see "
            "the tca9584a docs for the updated way to use multiplexers"
        ),
    }
    if default_address is None:
        schema[cv.Required(CONF_ADDRESS)] = cv.i2c_address
    else:
        schema[cv.Optional(CONF_ADDRESS, default=default_address)] = cv.i2c_address
    return cv.Schema(schema)


async def register_i2c_device(var, config):
    """Register an i2c device with the given config.

    Sets the i2c bus to use and the i2c address.

    This is a coroutine, you need to await it with a 'yield' expression!
    """
    parent = await cg.get_variable(config[CONF_I2C_ID])
    cg.add(var.set_i2c_bus(parent))
    cg.add(var.set_i2c_address(config[CONF_ADDRESS]))


def final_validate_device_schema(
    name: str, *, min_frequency: cv.frequency = None, max_frequency: cv.frequency = None
):
    hub_schema = {}
    if min_frequency is not None:
        hub_schema[cv.Required(CONF_FREQUENCY)] = cv.Range(
            min=cv.frequency(min_frequency),
            min_included=True,
            msg=f"Component {name} requires a minimum frequency of {min_frequency} for the I2C bus",
        )

    if max_frequency is not None:
        hub_schema[cv.Required(CONF_FREQUENCY)] = cv.Range(
            max=cv.frequency(max_frequency),
            max_included=True,
            msg=f"Component {name} cannot be used with a frequency of over {max_frequency} for the I2C bus",
        )

    return cv.Schema(
        {cv.Required(CONF_I2C_ID): fv.id_declaration_match_schema(hub_schema)},
        extra=cv.ALLOW_EXTRA,
    )


CONF_ONEWIRE_INDEX = "onewire_index" 
Accept the index in the onewire_id_pin_schema() schema:
python
Copy
def onewire_id_pin_schema(default_speed="slow"):
    schema = {
        cv.GenerateID(CONF_ONEWIRE_ID): cv.use_id(OneWireBus),
        cv.Optional(CONF_ONEWIRE_SPEED, default=default_speed): onewire_speed,
        cv.Optional(CONF_ONEWIRE_INDEX): cv.uint8_t, 
    }
    return cv.Schema(schema)
Pass the index to set_onewire_index() in register_onewire_device():
python
Copy
@coroutine_with_priority(1.0) 
async def register_onewire_device(var, config):
    parent = await cg.get_variable(config[CONF_ONEWIRE_ID])
    cg.add(var.set_onewire_bus(parent))
    cg.add(var.set_onewire_address(config[CONF_ONEWIRE_ADDRESS]))  
    if CONF_ONEWIRE_INDEX in config:
        cg.add(var.set_onewire_index(config[CONF_ONEWIRE_INDEX]))
Add a set_onewire_index() method to the OneWireDevice class:
python
Copy
OneWireDevice = onewire_ns.class_("OneWireDevice", cg.Component)

@OneWireDevice.set_onewire_index 
def set_onewire_index(self, index):
    pass  # Set index value 
This will allow you to specify an optional onewire_index for a one-wire device, and the code will call set_onewire_index() to pass that index to the device component.

Let me know if you have any other questions!


if index is not supplied address needs to be present?

Good catch, you're right - if an index is not supplied, an address should be required. To enforce this, you can make the following changes:

Make CONF_ONEWIRE_ADDRESS required in the schema:
python
Copy
def onewire_id_pin_schema(default_speed="slow"):
    schema = {
        cv.GenerateID(CONF_ONEWIRE_ID): cv.use_id(OneWireBus),
        cv.Optional(CONF_ONEWIRE_SPEED, default=default_speed): onewire_speed,
        cv.Optional(CONF_ONEWIRE_INDEX): cv.uint8_t, 
        cv.Required(CONF_ONEWIRE_ADDRESS): cv.hex_uint8_t, 
    }
    return cv.Schema(schema) 
In register_onewire_device(), make setting the address conditional on no index being specified:
python
Copy
@coroutine_with_priority(1.0) 
async def register_onewire_device(var, config):
    parent = await cg.get_variable(config[CONF_ONEWIRE_ID])
    cg.add(var.set_onewire_bus(parent))
    if CONF_ONEWIRE_INDEX not in config:
        cg.add(var.set_onewire_address(config[CONF_ONEWIRE_ADDRESS]))  
    if CONF_ONEWIRE_INDEX in config:
        cg.add(var.set_onewire_index(config[CONF_ONEWIRE_INDEX]))
Add a check in the final_validate_onewire_device_schema() to ensure either an index or address is specified:
python
Copy
def final_validate_onewire_device_schema(name):
    schema = cv.Schema(
        {
            cv.Required(CONF_ONEWIRE_ID): fv.id_declaration_match_schema(), 
            cv.Optional(CONF_ONEWIRE_INDEX): cv.any_int, 
            cv.Optional(CONF_ONEWIRE_ADDRESS): cv.any_string
        }, 
        extra=cv.ALLOW_EXTRA
    )
    def validate(config):
        if CONF_ONEWIRE_INDEX not in config and CONF_ONEWIRE_ADDRESS not in config:
            raise cv.Invalid("Must specify either 'onewire_index' or 'onewire_address'")
    schema = schema.validate_with(validate)
    return schema 
This will require either an index or address for one-wire devices, and only set the address if no index is specified. Let me know if you have any other questions!
