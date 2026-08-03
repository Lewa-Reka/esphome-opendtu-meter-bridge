// SPDX-License-Identifier: Apache-2.0

#include "meter_profile.h"

#include <cstring>

namespace esphome::opendtu_meter_bridge::profiles {

static constexpr uint16_t REGISTER_START = 0x0000;
static constexpr uint16_t REG_VOLTAGE_L1 = 0x0000;
static constexpr uint16_t REG_VOLTAGE_L2 = 0x0002;
static constexpr uint16_t REG_VOLTAGE_L3 = 0x0004;
static constexpr uint16_t REG_CURRENT_L1 = 0x0006;
static constexpr uint16_t REG_CURRENT_L2 = 0x0008;
static constexpr uint16_t REG_CURRENT_L3 = 0x000A;
static constexpr uint16_t REG_POWER_L1 = 0x000C;
static constexpr uint16_t REG_POWER_L2 = 0x000E;
static constexpr uint16_t REG_POWER_L3 = 0x0010;
static constexpr uint16_t REG_POWER_TOTAL = 0x0034;
static constexpr uint16_t REG_FREQUENCY = 0x0046;

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

void write_sdm630_profile(const MeterMeasurements &measurements, uint16_t *registers, size_t register_capacity) {
  set_float_(REG_VOLTAGE_L1, measurements.voltage[1], registers, register_capacity);
  set_float_(REG_VOLTAGE_L2, measurements.voltage[2], registers, register_capacity);
  set_float_(REG_VOLTAGE_L3, measurements.voltage[3], registers, register_capacity);
  set_float_(REG_CURRENT_L1, measurements.current[1], registers, register_capacity);
  set_float_(REG_CURRENT_L2, measurements.current[2], registers, register_capacity);
  set_float_(REG_CURRENT_L3, measurements.current[3], registers, register_capacity);
  set_float_(REG_POWER_L1, measurements.power[1], registers, register_capacity);
  set_float_(REG_POWER_L2, measurements.power[2], registers, register_capacity);
  set_float_(REG_POWER_L3, measurements.power[3], registers, register_capacity);
  set_float_(REG_POWER_TOTAL, measurements.total_power, registers, register_capacity);
  set_float_(REG_FREQUENCY, measurements.frequency, registers, register_capacity);
}

}  // namespace esphome::opendtu_meter_bridge::profiles
