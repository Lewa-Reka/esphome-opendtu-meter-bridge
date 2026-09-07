# SPDX-License-Identifier: Apache-2.0

import logging
from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    binary_sensor,
    button,
    esp32,
    modbus,
    sensor,
    text_sensor,
    wifi,
)
from esphome.components.modbus import CONF_MODBUS_ID
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_PASSWORD,
    CONF_PORT,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_RESTART,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_RESTART,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_KILOWATT_HOURS,
    UNIT_PERCENT,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)
from esphome.yaml_util import load_yaml

# ESPHome 2026.7 introduced the ModbusServer codegen type.  Earlier supported
# versions expose the same server configuration through Modbus.
MODBUS_SERVER_CLASS = getattr(modbus, "ModbusServer", modbus.Modbus)

_IDF_COMPONENTS_YML = Path(__file__).with_name("idf_component.yml")

CODEOWNERS = ["@local"]

AUTO_LOAD = ["binary_sensor", "button", "modbus", "sensor", "text_sensor"]
DEPENDENCIES = ["wifi"]
CONFLICTS_WITH = ["opendtu_sdm630"]

COMPONENT_VERSION = "0.1.0"

CONF_HOST = "host"
CONF_PATH = "path"
CONF_USERNAME = "username"
CONF_DATA_TIMEOUT = "data_timeout"
CONF_DEFAULT_VOLTAGE = "default_voltage"
CONF_DEFAULT_FREQUENCY = "default_frequency"
CONF_MICROINVERTER_MAP = "microinverter_map"
CONF_MICROINVERTER_SERIAL = "serial"
CONF_MICROINVERTER_NAME = "name"
CONF_GRID_PHASE = "grid_phase"
CONF_PUBLISH_SENSORS = "publish_sensors"
CONF_SLAVE_ADDRESS = "slave_address"
# Supported configuration values: sdm630 or dtsu666.
CONF_METER_PROFILE = "meter_profile"
# Legacy option retained for a transition period; it accepts the same two values.
CONF_METER_TYPE = "meter_type"

CONF_VOLTAGE_L1 = "voltage_l1"
CONF_VOLTAGE_L2 = "voltage_l2"
CONF_VOLTAGE_L3 = "voltage_l3"
CONF_CURRENT_L1 = "current_l1"
CONF_CURRENT_L2 = "current_l2"
CONF_CURRENT_L3 = "current_l3"
CONF_POWER_L1 = "power_l1"
CONF_POWER_L2 = "power_l2"
CONF_POWER_L3 = "power_l3"
CONF_TOTAL_POWER = "total_power"
CONF_REACTIVE_POWER_L1 = "reactive_power_l1"
CONF_REACTIVE_POWER_L2 = "reactive_power_l2"
CONF_REACTIVE_POWER_L3 = "reactive_power_l3"
CONF_TOTAL_REACTIVE_POWER = "total_reactive_power"
CONF_APPARENT_POWER_L1 = "apparent_power_l1"
CONF_APPARENT_POWER_L2 = "apparent_power_l2"
CONF_APPARENT_POWER_L3 = "apparent_power_l3"
CONF_TOTAL_APPARENT_POWER = "total_apparent_power"
CONF_POWER_FACTOR_L1 = "power_factor_l1"
CONF_POWER_FACTOR_L2 = "power_factor_l2"
CONF_POWER_FACTOR_L3 = "power_factor_l3"
CONF_TOTAL_POWER_FACTOR = "total_power_factor"
CONF_TOTAL_YIELD_DAY = "total_yield_day"
CONF_TOTAL_YIELD_TOTAL = "total_yield_total"
CONF_AVERAGE_TEMPERATURE = "average_temperature"
CONF_EFFICIENCY = "efficiency"
CONF_FREQUENCY = "frequency"
CONF_WEBSOCKET_CONNECTED = "websocket_connected"
CONF_DATA_VALID = "data_valid"
CONF_COMPONENT_VERSION = "component_version"
CONF_RESTART = "restart"

opendtu_meter_bridge_ns = cg.esphome_ns.namespace("opendtu_meter_bridge")
OpenDtuMeterBridge = opendtu_meter_bridge_ns.class_(
    "OpenDtuMeterBridge", cg.Component
)
RebootDeviceButton = opendtu_meter_bridge_ns.class_(
    "RebootDeviceButton", button.Button, cg.Component
)
MeterProfile = opendtu_meter_bridge_ns.enum("MeterProfile", is_class=True)
METER_PROFILES = {
    "sdm630": MeterProfile.SDM630,
    "dtsu666": MeterProfile.DTSU666,
}

_LOGGER = logging.getLogger(__name__)
_PASSWORD_SCHEMA = (
    cv.sensitive(cv.string) if hasattr(cv, "sensitive") else cv.string
)
MICROINVERTER_MAP_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Exclusive(CONF_MICROINVERTER_SERIAL, "microinverter_id"): cv.string,
            cv.Exclusive(CONF_MICROINVERTER_NAME, "microinverter_id"): cv.string,
            cv.Required(CONF_GRID_PHASE): cv.int_range(min=1, max=3),
        }
    ),
    cv.has_at_least_one_key(CONF_MICROINVERTER_SERIAL, CONF_MICROINVERTER_NAME),
)

VOLTAGE_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_VOLT,
    device_class=DEVICE_CLASS_VOLTAGE,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)

CURRENT_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_AMPERE,
    device_class=DEVICE_CLASS_CURRENT,
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
)

POWER_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_WATT,
    device_class=DEVICE_CLASS_POWER,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)

REACTIVE_POWER_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_VOLT_AMPS_REACTIVE,
    device_class=DEVICE_CLASS_REACTIVE_POWER,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)

APPARENT_POWER_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_VOLT_AMPS,
    device_class=DEVICE_CLASS_APPARENT_POWER,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)

POWER_FACTOR_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    device_class=DEVICE_CLASS_POWER_FACTOR,
    accuracy_decimals=3,
    state_class=STATE_CLASS_MEASUREMENT,
)

YIELD_DAY_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_WATT_HOURS,
    device_class=DEVICE_CLASS_ENERGY,
    accuracy_decimals=0,
    state_class=STATE_CLASS_TOTAL,
)

YIELD_TOTAL_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_KILOWATT_HOURS,
    device_class=DEVICE_CLASS_ENERGY,
    accuracy_decimals=3,
    state_class=STATE_CLASS_TOTAL_INCREASING,
)

TEMPERATURE_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_CELSIUS,
    device_class=DEVICE_CLASS_TEMPERATURE,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)

EFFICIENCY_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_PERCENT,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)

FREQUENCY_SENSOR_SCHEMA = sensor.sensor_schema(
    sensor.Sensor,
    unit_of_measurement=UNIT_HERTZ,
    device_class=DEVICE_CLASS_FREQUENCY,
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
)

BINARY_CONNECTIVITY_SCHEMA = binary_sensor.binary_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    device_class=DEVICE_CLASS_CONNECTIVITY,
)

BINARY_DATA_VALID_SCHEMA = binary_sensor.binary_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)

COMPONENT_VERSION_TEXT_SENSOR_SCHEMA = text_sensor.text_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    icon="mdi:tag-outline",
)

RESTART_BUTTON_SCHEMA = button.button_schema(
    RebootDeviceButton,
    device_class=DEVICE_CLASS_RESTART,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    icon=ICON_RESTART,
)

SENSOR_DEFAULTS = {
    CONF_VOLTAGE_L1: ("L1 Voltage", VOLTAGE_SENSOR_SCHEMA),
    CONF_VOLTAGE_L2: ("L2 Voltage", VOLTAGE_SENSOR_SCHEMA),
    CONF_VOLTAGE_L3: ("L3 Voltage", VOLTAGE_SENSOR_SCHEMA),
    CONF_CURRENT_L1: ("L1 Current", CURRENT_SENSOR_SCHEMA),
    CONF_CURRENT_L2: ("L2 Current", CURRENT_SENSOR_SCHEMA),
    CONF_CURRENT_L3: ("L3 Current", CURRENT_SENSOR_SCHEMA),
    CONF_POWER_L1: ("L1 Power", POWER_SENSOR_SCHEMA),
    CONF_POWER_L2: ("L2 Power", POWER_SENSOR_SCHEMA),
    CONF_POWER_L3: ("L3 Power", POWER_SENSOR_SCHEMA),
    CONF_TOTAL_POWER: ("Total Power", POWER_SENSOR_SCHEMA),
    CONF_REACTIVE_POWER_L1: ("L1 Reactive Power", REACTIVE_POWER_SENSOR_SCHEMA),
    CONF_REACTIVE_POWER_L2: ("L2 Reactive Power", REACTIVE_POWER_SENSOR_SCHEMA),
    CONF_REACTIVE_POWER_L3: ("L3 Reactive Power", REACTIVE_POWER_SENSOR_SCHEMA),
    CONF_TOTAL_REACTIVE_POWER: ("Total Reactive Power", REACTIVE_POWER_SENSOR_SCHEMA),
    CONF_APPARENT_POWER_L1: ("L1 Apparent Power", APPARENT_POWER_SENSOR_SCHEMA),
    CONF_APPARENT_POWER_L2: ("L2 Apparent Power", APPARENT_POWER_SENSOR_SCHEMA),
    CONF_APPARENT_POWER_L3: ("L3 Apparent Power", APPARENT_POWER_SENSOR_SCHEMA),
    CONF_TOTAL_APPARENT_POWER: ("Total Apparent Power", APPARENT_POWER_SENSOR_SCHEMA),
    CONF_POWER_FACTOR_L1: ("L1 Power Factor", POWER_FACTOR_SENSOR_SCHEMA),
    CONF_POWER_FACTOR_L2: ("L2 Power Factor", POWER_FACTOR_SENSOR_SCHEMA),
    CONF_POWER_FACTOR_L3: ("L3 Power Factor", POWER_FACTOR_SENSOR_SCHEMA),
    CONF_TOTAL_POWER_FACTOR: ("Total Power Factor", POWER_FACTOR_SENSOR_SCHEMA),
    CONF_TOTAL_YIELD_DAY: ("Total Yield Today", YIELD_DAY_SENSOR_SCHEMA),
    CONF_TOTAL_YIELD_TOTAL: ("Total Yield", YIELD_TOTAL_SENSOR_SCHEMA),
    CONF_AVERAGE_TEMPERATURE: ("Average Inverter Temperature", TEMPERATURE_SENSOR_SCHEMA),
    CONF_EFFICIENCY: ("Inverter Efficiency", EFFICIENCY_SENSOR_SCHEMA),
    CONF_FREQUENCY: ("Frequency", FREQUENCY_SENSOR_SCHEMA),
    CONF_WEBSOCKET_CONNECTED: ("WebSocket Status", BINARY_CONNECTIVITY_SCHEMA),
    CONF_DATA_VALID: ("WebSocket Data Valid", BINARY_DATA_VALID_SCHEMA),
}

SENSOR_SETTERS = {
    CONF_VOLTAGE_L1: "set_voltage_l1_sensor",
    CONF_VOLTAGE_L2: "set_voltage_l2_sensor",
    CONF_VOLTAGE_L3: "set_voltage_l3_sensor",
    CONF_CURRENT_L1: "set_current_l1_sensor",
    CONF_CURRENT_L2: "set_current_l2_sensor",
    CONF_CURRENT_L3: "set_current_l3_sensor",
    CONF_POWER_L1: "set_power_l1_sensor",
    CONF_POWER_L2: "set_power_l2_sensor",
    CONF_POWER_L3: "set_power_l3_sensor",
    CONF_TOTAL_POWER: "set_total_power_sensor",
    CONF_REACTIVE_POWER_L1: "set_reactive_power_l1_sensor",
    CONF_REACTIVE_POWER_L2: "set_reactive_power_l2_sensor",
    CONF_REACTIVE_POWER_L3: "set_reactive_power_l3_sensor",
    CONF_TOTAL_REACTIVE_POWER: "set_total_reactive_power_sensor",
    CONF_APPARENT_POWER_L1: "set_apparent_power_l1_sensor",
    CONF_APPARENT_POWER_L2: "set_apparent_power_l2_sensor",
    CONF_APPARENT_POWER_L3: "set_apparent_power_l3_sensor",
    CONF_TOTAL_APPARENT_POWER: "set_total_apparent_power_sensor",
    CONF_POWER_FACTOR_L1: "set_power_factor_l1_sensor",
    CONF_POWER_FACTOR_L2: "set_power_factor_l2_sensor",
    CONF_POWER_FACTOR_L3: "set_power_factor_l3_sensor",
    CONF_TOTAL_POWER_FACTOR: "set_total_power_factor_sensor",
    CONF_TOTAL_YIELD_DAY: "set_total_yield_day_sensor",
    CONF_TOTAL_YIELD_TOTAL: "set_total_yield_total_sensor",
    CONF_AVERAGE_TEMPERATURE: "set_average_temperature_sensor",
    CONF_EFFICIENCY: "set_efficiency_sensor",
    CONF_FREQUENCY: "set_frequency_sensor",
    CONF_WEBSOCKET_CONNECTED: "set_websocket_connected_binary_sensor",
    CONF_DATA_VALID: "set_data_valid_binary_sensor",
}


def _ensure_default_entities(config):
    if config.get(CONF_PUBLISH_SENSORS, True):
        for key, (default_name, schema) in SENSOR_DEFAULTS.items():
            if key not in config:
                config[key] = schema({CONF_NAME: default_name})
    if CONF_RESTART not in config:
        config[CONF_RESTART] = RESTART_BUTTON_SCHEMA({CONF_NAME: "Board Restart"})
    if CONF_COMPONENT_VERSION not in config:
        config[CONF_COMPONENT_VERSION] = COMPONENT_VERSION_TEXT_SENSOR_SCHEMA(
            {CONF_NAME: "Component Version"}
        )
    return config


def _set_default_meter_profile(config):
    if CONF_METER_PROFILE not in config and CONF_METER_TYPE not in config:
        config[CONF_METER_PROFILE] = "sdm630"
    return config


def _normalize_meter_profile(config):
    if CONF_METER_TYPE in config:
        _LOGGER.warning(
            "The 'meter_type' option is deprecated; use 'meter_profile' instead "
            "(allowed values: sdm630, dtsu666)."
        )
        config[CONF_METER_PROFILE] = config.pop(CONF_METER_TYPE)
    return config


CONFIG_SCHEMA = cv.All(
    _set_default_meter_profile,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OpenDtuMeterBridge),
            cv.Required(CONF_HOST): cv.string,
            cv.Optional(CONF_PORT, default=80): cv.port,
            cv.Optional(CONF_PATH, default="/livedata"): cv.string,
            cv.Optional(CONF_USERNAME, default="admin"): cv.string,
            cv.Required(CONF_PASSWORD): _PASSWORD_SCHEMA,
            cv.Required(CONF_MODBUS_ID): cv.use_id(MODBUS_SERVER_CLASS),
            cv.Optional(CONF_SLAVE_ADDRESS, default=0x02): cv.All(
                cv.hex_uint8_t, cv.int_range(min=1, max=247)
            ),
            cv.Exclusive(CONF_METER_PROFILE, "meter_profile_option"): cv.enum(
                METER_PROFILES, lower=True
            ),
            cv.Exclusive(CONF_METER_TYPE, "meter_profile_option"): cv.enum(
                METER_PROFILES, lower=True
            ),
            cv.Optional(CONF_DATA_TIMEOUT, default="15s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DEFAULT_VOLTAGE, default=230.0): cv.float_,
            cv.Optional(CONF_DEFAULT_FREQUENCY, default=50.0): cv.float_,
            cv.Required(CONF_MICROINVERTER_MAP): cv.ensure_list(MICROINVERTER_MAP_SCHEMA),
            cv.Optional(CONF_PUBLISH_SENSORS, default=True): cv.boolean,
            cv.Optional(CONF_VOLTAGE_L1): VOLTAGE_SENSOR_SCHEMA,
            cv.Optional(CONF_VOLTAGE_L2): VOLTAGE_SENSOR_SCHEMA,
            cv.Optional(CONF_VOLTAGE_L3): VOLTAGE_SENSOR_SCHEMA,
            cv.Optional(CONF_CURRENT_L1): CURRENT_SENSOR_SCHEMA,
            cv.Optional(CONF_CURRENT_L2): CURRENT_SENSOR_SCHEMA,
            cv.Optional(CONF_CURRENT_L3): CURRENT_SENSOR_SCHEMA,
            cv.Optional(CONF_POWER_L1): POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_POWER_L2): POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_POWER_L3): POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_TOTAL_POWER): POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_REACTIVE_POWER_L1): REACTIVE_POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_REACTIVE_POWER_L2): REACTIVE_POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_REACTIVE_POWER_L3): REACTIVE_POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_TOTAL_REACTIVE_POWER): REACTIVE_POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_APPARENT_POWER_L1): APPARENT_POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_APPARENT_POWER_L2): APPARENT_POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_APPARENT_POWER_L3): APPARENT_POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_TOTAL_APPARENT_POWER): APPARENT_POWER_SENSOR_SCHEMA,
            cv.Optional(CONF_POWER_FACTOR_L1): POWER_FACTOR_SENSOR_SCHEMA,
            cv.Optional(CONF_POWER_FACTOR_L2): POWER_FACTOR_SENSOR_SCHEMA,
            cv.Optional(CONF_POWER_FACTOR_L3): POWER_FACTOR_SENSOR_SCHEMA,
            cv.Optional(CONF_TOTAL_POWER_FACTOR): POWER_FACTOR_SENSOR_SCHEMA,
            cv.Optional(CONF_TOTAL_YIELD_DAY): YIELD_DAY_SENSOR_SCHEMA,
            cv.Optional(CONF_TOTAL_YIELD_TOTAL): YIELD_TOTAL_SENSOR_SCHEMA,
            cv.Optional(CONF_AVERAGE_TEMPERATURE): TEMPERATURE_SENSOR_SCHEMA,
            cv.Optional(CONF_EFFICIENCY): EFFICIENCY_SENSOR_SCHEMA,
            cv.Optional(CONF_FREQUENCY): FREQUENCY_SENSOR_SCHEMA,
            cv.Optional(CONF_WEBSOCKET_CONNECTED): BINARY_CONNECTIVITY_SCHEMA,
            cv.Optional(CONF_DATA_VALID): BINARY_DATA_VALID_SCHEMA,
            cv.Optional(CONF_COMPONENT_VERSION): COMPONENT_VERSION_TEXT_SENSOR_SCHEMA,
            cv.Optional(CONF_RESTART): RESTART_BUTTON_SCHEMA,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _normalize_meter_profile,
    _ensure_default_entities,
    cv.only_on_esp32,
)

FINAL_VALIDATE_SCHEMA = modbus.final_validate_modbus_device(
    "opendtu_meter_bridge", role="server"
)


def _register_idf_components():
    data = load_yaml(_IDF_COMPONENTS_YML) or {}
    for name, ref in (data.get("dependencies") or {}).items():
        try:
            # Newer ESPHome releases accept ESP Component Registry identifiers directly.
            esp32.add_idf_component(name=name, ref=str(ref))
        except TypeError:
            # Older supported ESPHome releases require an explicit Git source.
            if name != "espressif/esp_websocket_client":
                raise
            esp32.add_idf_component(
                name="esp_websocket_client",
                repo="https://github.com/espressif/esp-protocols.git",
                ref="70bf122fc8bc74622dcf7e15233b5b2af9088df5",
                path="components/esp_websocket_client",
            )


async def to_code(config):
    if hasattr(wifi, "request_wifi_connect_state_listener"):
        wifi.request_wifi_connect_state_listener()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    modbus_parent = await cg.get_variable(config[CONF_MODBUS_ID])
    cg.add(var.set_modbus_server(modbus_parent, config[CONF_SLAVE_ADDRESS]))

    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_path(config[CONF_PATH]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_data_timeout_ms(config[CONF_DATA_TIMEOUT]))
    cg.add(var.set_default_voltage(config[CONF_DEFAULT_VOLTAGE]))
    cg.add(var.set_default_frequency(config[CONF_DEFAULT_FREQUENCY]))
    cg.add(var.set_meter_profile(config[CONF_METER_PROFILE]))
    cg.add(var.set_component_version(COMPONENT_VERSION))

    for entry in config[CONF_MICROINVERTER_MAP]:
        grid_phase = entry[CONF_GRID_PHASE]
        if CONF_MICROINVERTER_SERIAL in entry:
            cg.add(
                var.add_microinverter_map_by_serial(
                    entry[CONF_MICROINVERTER_SERIAL], grid_phase
                )
            )
        else:
            cg.add(
                var.add_microinverter_map_by_name(
                    entry[CONF_MICROINVERTER_NAME], grid_phase
                )
            )

    if config.get(CONF_PUBLISH_SENSORS, True):
        for key, setter in SENSOR_SETTERS.items():
            if key not in config:
                continue
            if key in (CONF_WEBSOCKET_CONNECTED, CONF_DATA_VALID):
                ent = await binary_sensor.new_binary_sensor(config[key])
            else:
                ent = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(ent))

    if CONF_RESTART in config:
        restart_btn = await button.new_button(config[CONF_RESTART])
        await cg.register_component(restart_btn, config[CONF_RESTART])

    if CONF_COMPONENT_VERSION in config:
        version_ts = await text_sensor.new_text_sensor(config[CONF_COMPONENT_VERSION])
        cg.add(var.set_component_version_text_sensor(version_ts))

    _register_idf_components()
