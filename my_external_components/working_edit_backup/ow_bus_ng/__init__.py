# import voluptuous as vol
import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import uart
from esphome.const import CONF_ID, CONF_PIN

# from voluptuous import Required, Optional, Any, All, Lower, In

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
        cv.Optional(CONF_PIN): pins.gpio_input_pin_schema,
        cv.Optional("input_pin"): pins.gpio_input_pin_schema,
        cv.Optional("output_pin"): pins.gpio_output_pin_schema,
        cv.Optional("mode", default="bitbang_single"): cv.enum(
            {
                "BITBANG_SINGLE": "bitbang_single",
                "BITBANG_SPLIT_IO": "bitbang_split_io",
                # "UART_BUS_SINGLE_PIN": "uart_single_pin",
                "UART_BUS": "uart_bus",
            },
            upper=True,
        ),
        cv.Optional("uart"): uart.UART_DEVICE_SCHEMA,
    }
)


async def setup_bitbang_single(var, config):
    pin = await cg.gpio_pin_expression(config["pin"])
    cg.add(var.set_single_pin(pin))


async def setup_bitbang_split_io(var, config):
    in_pin = await cg.gpio_pin_expression(config["input_pin"])
    out_pin = await cg.gpio_pin_expression(config["output_pin"])
    cg.add(var.set_split_io(in_pin, out_pin))


# async def setup_uart_single_pin(var, config):
#     if "uart" in config:
#         await uart.register_uart_device(var, config["uart"])
#         uart_conf = config["uart"]
#         cg.add(
#             var.set_uart(
#                 uart_conf["tx_pin"], uart_conf["rx_pin"], uart_conf["baud_rate"]
#             )
#         )
#     else:
#         _LOGGER.error("uart: not specified for uart_half_duplex mode!")


async def setup_uart_full_duplex(var, config):
    if "uart" in config:
        await uart.register_uart_device(var, config["uart"])
        uart_conf = config["uart"]
        cg.add(
            var.set_uart(
                uart_conf["tx_pin"], uart_conf["rx_pin"], uart_conf["baud_rate"]
            )
        )
    else:
        _LOGGER.error("uart not provided for uart_full_duplex!")


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], ESPHomeOneWireNGComponent())

    mode = config.get("mode", "bitbang_single")
    if mode == "bitbang_single":
        await setup_bitbang_single(var, config)
    elif mode == "bitbang_split_io":
        await setup_bitbang_split_io(var, config)
    #   elif mode == "uart_single_pin" and "uart" in config:
    #     await setup_uart_single_pin(var, config)
    elif mode == "uart_full_duplex" and "uart" in config:
        await setup_uart_full_duplex(var, config)
    else:
        _LOGGER.error("Invalid mode for single pin: %s", mode)
        return

    await cg.register_component(var, config)
