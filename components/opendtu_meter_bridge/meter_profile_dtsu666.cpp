// SPDX-License-Identifier: Apache-2.0

#include "meter_profile.h"

#include <cmath>
#include <cstring>

namespace esphome::opendtu_meter_bridge::profiles {

static constexpr uint16_t REGISTER_START = 0x2000;
static constexpr uint16_t REG_LINE_VOLTAGE_AB = 0x2000;
static constexpr uint16_t REG_LINE_VOLTAGE_BC = 0x2002;
static constexpr uint16_t REG_LINE_VOLTAGE_CA = 0x2004;
static constexpr uint16_t REG_VOLTAGE_L1 = 0x2006;
static constexpr uint16_t REG_VOLTAGE_L2 = 0x2008;
static constexpr uint16_t REG_VOLTAGE_L3 = 0x200A;
static constexpr uint16_t REG_CURRENT_L1 = 0x200C;
static constexpr uint16_t REG_CURRENT_L2 = 0x200E;
static constexpr uint16_t REG_CURRENT_L3 = 0x2010;
static constexpr uint16_t REG_POWER_TOTAL = 0x2012;
static constexpr uint16_t REG_POWER_L1 = 0x2014;
static constexpr uint16_t REG_POWER_L2 = 0x2016;
static constexpr uint16_t REG_POWER_L3 = 0x2018;
static constexpr uint16_t REG_FREQUENCY = 0x2044;

static void set_float_(uint16_t address, float value, uint16_t *registers, size_t register_capacity) {
  const size_t offset = address - REGISTER_START;
  if (offset + 1 >= register_capacity) {
    return;
  }
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  registers[offset] = static_cast<uint16_t>(bits >> 16);
  registers[offset + 1] = static_cast<uint16_t>(bits & 0xFFFFu);
}

static float line_voltage_(float first_phase_voltage, float second_phase_voltage) {
  // 120-degree phase displacement: |U1-U2| = sqrt(U1^2 + U2^2 + U1*U2).
  return std::sqrt(first_phase_voltage * first_phase_voltage + second_phase_voltage * second_phase_voltage +
                   first_phase_voltage * second_phase_voltage);
}

void write_dtsu666_profile(const MeterMeasurements &measurements, uint16_t *registers, size_t register_capacity) {
  set_float_(REG_LINE_VOLTAGE_AB, line_voltage_(measurements.voltage[1], measurements.voltage[2]), registers,
             register_capacity);
  set_float_(REG_LINE_VOLTAGE_BC, line_voltage_(measurements.voltage[2], measurements.voltage[3]), registers,
             register_capacity);
  set_float_(REG_LINE_VOLTAGE_CA, line_voltage_(measurements.voltage[3], measurements.voltage[1]), registers,
             register_capacity);
  set_float_(REG_VOLTAGE_L1, measurements.voltage[1], registers, register_capacity);
  set_float_(REG_VOLTAGE_L2, measurements.voltage[2], registers, register_capacity);
  set_float_(REG_VOLTAGE_L3, measurements.voltage[3], registers, register_capacity);
  set_float_(REG_CURRENT_L1, measurements.current[1], registers, register_capacity);
  set_float_(REG_CURRENT_L2, measurements.current[2], registers, register_capacity);
  set_float_(REG_CURRENT_L3, measurements.current[3], registers, register_capacity);
  set_float_(REG_POWER_TOTAL, measurements.total_power, registers, register_capacity);
  set_float_(REG_POWER_L1, measurements.power[1], registers, register_capacity);
  set_float_(REG_POWER_L2, measurements.power[2], registers, register_capacity);
  set_float_(REG_POWER_L3, measurements.power[3], registers, register_capacity);
  set_float_(REG_FREQUENCY, measurements.frequency, registers, register_capacity);
}

}  // namespace esphome::opendtu_meter_bridge::profiles
