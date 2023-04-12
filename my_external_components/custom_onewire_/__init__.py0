import esphome.config_validation as cv
from esphome import pins
from esphome.components import custom_onewire
from esphome.const import CONF_PIN, CONF_IN_PIN, CONF_OUT_PIN, \
    CONF_LOW_POWER_MODE, CONF_OVERDRIVE_MODE, CONF_PARASITIC_POWER_MODE, \
    CONF_MODE, CONF_INPUT_SELECT, CONF_NAME, CONF_OPTIONS, CONF_INITIAL, \
    ICON_FLASH

_LOGGER = logging.getLogger(__name__)

# DEPENDENCIES = ["custom_onewire"]
CODEOWNERS = ["@ashp8i"]
AUTO_LOAD = ["custom_onewire"]

CUSTOM_ONEWIRE_SCHEMA = cv.Schema({
    cv.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
    cv.Optional(CONF_IN_PIN): pins.internal_gpio_input_pin_schema,
    cv.Optional(CONF_OUT_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_LOW_POWER_MODE, default=False): cv.boolean,
    cv.Optional(CONF_OVERDRIVE_MODE, default=False): cv.boolean,
    cv.Optional(CONF_PARASITIC_POWER_MODE, default=False): cv.boolean,
    cv.Required(CONF_MODE): cv.enum(
        {
            MODE_NORMAL: cv.string,
            MODE_LOW_POWER: cv.string,
            MODE_OVERDRIVE: cv.string,
        }
    ),
    cv.Required(CONF_INPUT_SELECT): cv.Schema({
        cv.Required(CONF_NAME): cv.string,
        cv.Required(CONF_OPTIONS): {
            MODE_NORMAL: cv.string,
            MODE_LOW_POWER: cv.string,
            MODE_OVERDRIVE: cv.string,
        },
        cv.Required(CONF_INITIAL): cv.one_of(
            MODE_NORMAL, MODE_LOW_POWER, MODE_OVERDRIVE
        ),
        cv.Optional(CONF_ICON, default=ICON_FLASH): cv.icon,
    })
})

class CustomOneWire(custom_onewire.CustomOneWire):
    def __init__(self, pin, in_pin, out_pin, low_power_mode, overdrive_mode, parasitic_power_mode, mode, input_select):
        if mode == "overdrive":
            super().__init__(pin=pin, in_pin=None, out_pin=None, low_power_mode=False, overdrive_mode=True, parasitic_power_mode=False)
        elif mode == "low_power":
            super().__init__(pin=pin, in_pin=None, out_pin=None, low_power_mode=True, overdrive_mode=False, parasitic_power_mode=False)
        else:
            super().__init__(pin=pin, in_pin=None, out_pin=None, low_power_mode=False, overdrive_mode=False, parasitic_power_mode=False)

        if input_select is not None:
            self.add_input_select(input_select, {
                mode: mode.capitalize() + " Mode",
                "normal": "Normal Mode"
            })

        _LOGGER.info("Initialized Custom OneWire")

def to_code(config):
    pin = yield pins.internal_gpio_pin_expression(config[CONF_PIN])
    in_pin = None
    out_pin = None
    if CONF_IN_PIN in config and CONF_OUT_PIN in config:
        in_pin = yield pins.internal_gpio_pin_expression(config[CONF_IN_PIN])
        out_pin = yield pins.internal_gpio_pin_expression(config[CONF_OUT_PIN])

    low_power_mode = config[CONF_LOW_POWER_MODE]
    overdrive_mode = config[CONF_OVERDRIVE_MODE]
    parasitic_power_mode = config[CONF_PARASITIC_POWER_MODE]
    mode = config.get(CONF_MODE, "normal")
    input_select = config.get(CONF_INPUT_SELECT)

    yield CustomOneWire(pin, in_pin, out_pin, low_power_mode, overdrive_mode, parasitic_power_mode, mode, input_select)