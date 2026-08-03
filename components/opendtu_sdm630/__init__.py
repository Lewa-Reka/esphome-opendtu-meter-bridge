# SPDX-License-Identifier: Apache-2.0

"""Compatibility wrapper for the legacy ``opendtu_sdm630`` domain."""

import importlib.util
import logging
from pathlib import Path
import sys

import esphome.config_validation as cv
from esphome.components import modbus

_IMPLEMENTATION_MODULE = "esphome.components.opendtu_meter_bridge"
_IMPLEMENTATION_DIR = Path(__file__).resolve().parent.parent / "opendtu_meter_bridge"

_implementation = sys.modules.get(_IMPLEMENTATION_MODULE)
if _implementation is None:
    _spec = importlib.util.spec_from_file_location(
        _IMPLEMENTATION_MODULE,
        _IMPLEMENTATION_DIR / "__init__.py",
        submodule_search_locations=[str(_IMPLEMENTATION_DIR)],
    )
    if _spec is None or _spec.loader is None:
        raise ImportError("Cannot load the opendtu_meter_bridge compatibility target")
    _implementation = importlib.util.module_from_spec(_spec)
    sys.modules[_IMPLEMENTATION_MODULE] = _implementation
    _spec.loader.exec_module(_implementation)

_LOGGER = logging.getLogger(__name__)


def _warn_legacy_domain(config):
    _LOGGER.warning(
        "The 'opendtu_sdm630' component domain is deprecated; migrate to "
        "'opendtu_meter_bridge' (meter_profile values: sdm630 or dtsu666)."
    )
    return config


CODEOWNERS = _implementation.CODEOWNERS
AUTO_LOAD = _implementation.AUTO_LOAD
DEPENDENCIES = _implementation.DEPENDENCIES
CONFLICTS_WITH = ["opendtu_meter_bridge"]

CONFIG_SCHEMA = cv.All(_implementation.CONFIG_SCHEMA, _warn_legacy_domain)
FINAL_VALIDATE_SCHEMA = modbus.final_validate_modbus_device(
    "opendtu_sdm630", role="server"
)
to_code = _implementation.to_code

# ESPHome copies C++ resources from the manifest package. Point the legacy
# manifest at the neutral implementation so the compatibility layer does not
# duplicate or fork the C++ sources.
__package__ = _IMPLEMENTATION_MODULE
