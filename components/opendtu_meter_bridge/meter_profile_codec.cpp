// SPDX-License-Identifier: Apache-2.0

#include "meter_profile_codec.h"

#include <cmath>
#include <cstring>

namespace esphome::opendtu_meter_bridge::profiles::internal {

static float line_voltage_(float first_phase_voltage, float second_phase_voltage) {
  // 120-degree phase displacement: |U1-U2| = sqrt(U1^2 + U2^2 + U1*U2).
  return std::sqrt(first_phase_voltage * first_phase_voltage + second_phase_voltage * second_phase_voltage +
                   first_phase_voltage * second_phase_voltage);
}

static float measurement_value_(MeasurementSource source, const MeterMeasurements &measurements) {
  switch (source) {
    case MeasurementSource::VOLTAGE_L1:
      return measurements.voltage[1];
    case MeasurementSource::VOLTAGE_L2:
      return measurements.voltage[2];
    case MeasurementSource::VOLTAGE_L3:
      return measurements.voltage[3];
    case MeasurementSource::DIRECTIONAL_CURRENT_L1:
      return measurements.current[1];
    case MeasurementSource::DIRECTIONAL_CURRENT_L2:
      return measurements.current[2];
    case MeasurementSource::DIRECTIONAL_CURRENT_L3:
      return measurements.current[3];
    case MeasurementSource::CURRENT_RMS_L1:
      return std::fabs(measurements.current[1]);
    case MeasurementSource::CURRENT_RMS_L2:
      return std::fabs(measurements.current[2]);
    case MeasurementSource::CURRENT_RMS_L3:
      return std::fabs(measurements.current[3]);
    case MeasurementSource::POWER_L1:
      return measurements.power[1];
    case MeasurementSource::POWER_L2:
      return measurements.power[2];
    case MeasurementSource::POWER_L3:
      return measurements.power[3];
    case MeasurementSource::TOTAL_POWER:
      return measurements.total_power;
    case MeasurementSource::FREQUENCY:
      return measurements.frequency;
    case MeasurementSource::LINE_VOLTAGE_AB:
      return line_voltage_(measurements.voltage[1], measurements.voltage[2]);
    case MeasurementSource::LINE_VOLTAGE_BC:
      return line_voltage_(measurements.voltage[2], measurements.voltage[3]);
    case MeasurementSource::LINE_VOLTAGE_CA:
      return line_voltage_(measurements.voltage[3], measurements.voltage[1]);
  }
  return 0.0f;
}

static void write_float_abcd_(uint16_t register_start, uint16_t address, float value, uint16_t *registers,
                              size_t register_capacity) {
  if (registers == nullptr || address < register_start) {
    return;
  }

  const size_t offset = static_cast<size_t>(address - register_start);
  if (offset >= register_capacity || register_capacity - offset < 2) {
    return;
  }

  static_assert(sizeof(float) == sizeof(uint32_t), "Meter profiles require 32-bit IEEE-754 floats");
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  registers[offset] = static_cast<uint16_t>(bits >> 16);
  registers[offset + 1] = static_cast<uint16_t>(bits & 0xFFFFu);
}

void write_float_registers(uint16_t register_start, const FloatRegisterDescriptor *descriptors,
                           size_t descriptor_count, const MeterMeasurements &measurements, uint16_t *registers,
                           size_t register_capacity) {
  if (descriptors == nullptr || registers == nullptr || register_capacity == 0) {
    return;
  }

  for (size_t index = 0; index < descriptor_count; index++) {
    const auto &descriptor = descriptors[index];
    write_float_abcd_(register_start, descriptor.address, measurement_value_(descriptor.source, measurements),
                      registers, register_capacity);
  }
}

}  // namespace esphome::opendtu_meter_bridge::profiles::internal
