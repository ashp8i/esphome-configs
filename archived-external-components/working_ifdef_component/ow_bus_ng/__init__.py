import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import uart
from esphome.const import CONF_ID, CONF_PIN, CONF_BAUD_RATE

DEPENDENCIES = ["uart"]

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
        cv.Optional("conf_pin"): pins.gpio_input_pin_schema,
        cv.Optional("input_pin"): pins.gpio_input_pin_schema,
        cv.Optional("output_pin"): pins.gpio_output_pin_schema,
        cv.Optional("mode", default="bitbang_single"): cv.enum(
            {
                "BITBANG_SINGLE": "bitbang_single",
                "BITBANG_SPLIT_IO": "bitbang_split_io",
                #   "UART_BUS_SINGLE_PIN": "uart_bus_single_pin",
                "UART_BUS": "uart_bus",
            },
            upper=True,
        ),
        cv.Optional("uart"): cv.Schema(
            {
                cv.Required("tx_pin"): pins.gpio_output_pin_schema,
                cv.Required("rx_pin"): pins.gpio_input_pin_schema,
                cv.Optional("baud_rate", default=115200): cv.positive_int,
            }
        ),
    }
)


async def setup_bitbang_single(var, config):
    pin = await cg.gpio_pin_expression(config["conf_pin"])
    cg.add(var.set_single_pin(pin))


async def setup_bitbang_single(var, config):
    pin = await cg.gpio_pin_expression(config["pin"])
    cg.add(var.set_single_pin(pin))


async def setup_bitbang_split_io(var, config):
    in_pin = await cg.gpio_pin_expression(config["input_pin"])
    out_pin = await cg.gpio_pin_expression(config["output_pin"])
    cg.add(var.set_split_io(in_pin, out_pin))


# async def setup_uart_half_duplex(var, config):
#   if "uart" in config:
#     await uart.register_uart_device(var, config["uart"])
#     uart_conf = config["uart"]
#     cg.add(var.set_uart(uart_conf["tx_pin"], uart_conf["rx_pin"], uart_conf["baud_rate"]))
#   elif "uart_id" in config:
#     var.uart_bus = uart.ESP_PLATFORM.get_uart_instance(config["uart_id"])
#   else:
#     uart_buses = uart.ESP_PLATFORM.get_uart_instances()
#     var.uart_bus = uart_buses[0]


async def setup_uart_full_duplex(var, config):
    if "uart" in config:
        await uart.register_uart_device(var, config["uart"])
        uart_conf = config["uart"]
        cg.add(
            var.set_uart(
                uart_conf["tx_pin"], uart_conf["rx_pin"], uart_conf["baud_rate"]
            )
        )
    elif "uart_id" in config:
        var.uart_bus = uart.ESP_PLATFORM.get_uart_instance(config["uart_id"])
    else:
        uart_buses = uart.ESP_PLATFORM.get_uart_instances()
        var.uart_bus = uart_buses[0]


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], ESPHomeOneWireNGComponent())
    mode = config["mode"]
    if mode == "BITBANG_SINGLE":
        cg.add_define("USE_BITBANG_SINGLE")
    elif mode == "BITBANG_SPLIT_IO":
        cg.add_define("USE_BITBANG_SPLIT_IO")
    elif mode == "UART_BUS":
        cg.add_define("USE_UART_BUS")
    else:
        _LOGGER.error("Invalid mode for ow_bus_ng: %s", mode)
        return

    await cg.register_component(var, config)
