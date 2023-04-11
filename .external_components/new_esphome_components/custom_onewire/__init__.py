import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome.core import coroutine
from . import new_esponewire_ns

# ...

DS28E17Component = new_esponewire_ns.class_("DS28E17Component", cg.Component)
NewESPOneWire = new_esponewire_ns.class_("NewESPOneWire", cg.Component)

CONF_ONE_WIRE_ID = "one_wire_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DS28E17Component),
        cv.GenerateID(CONF_ONE_WIRE_ID): cv.use_id(NewESPOneWire),
    }
).extend(cv.COMPONENT_SCHEMA)

# ...

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    one_wire = yield cg.get_variable(config[CONF_ONE_WIRE_ID])
    cg.add(one_wire.register_ds28e17(var))


CODEOWNERS = ["@yourusername"]
AUTO_LOAD = ["custom_onewire"]