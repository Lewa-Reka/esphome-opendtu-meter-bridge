// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "meter_profile.h"

#include <cstddef>
#include <cstdint>

namespace esphome::opendtu_meter_bridge::profiles::internal {

enum class MeasurementSource : uint8_t {
  VOLTAGE_L1,
  VOLTAGE_L2,
  VOLTAGE_L3,
  DIRECTIONAL_CURRENT_L1,
  DIRECTIONAL_CURRENT_L2,
  DIRECTIONAL_CURRENT_L3,
  CURRENT_RMS_L1,
  CURRENT_RMS_L2,
  CURRENT_RMS_L3,
  POWER_L1,
  POWER_L2,
  POWER_L3,
  TOTAL_POWER,
  FREQUENCY,
  LINE_VOLTAGE_AB,
  LINE_VOLTAGE_BC,
  LINE_VOLTAGE_CA,
};

struct FloatRegisterDescriptor {
  uint16_t address;
  MeasurementSource source;
};

void write_float_registers(uint16_t register_start, const FloatRegisterDescriptor *descriptors,
                           size_t descriptor_count, const MeterMeasurements &measurements, uint16_t *registers,
                           size_t register_capacity);

}  // namespace esphome::opendtu_meter_bridge::profiles::internal
