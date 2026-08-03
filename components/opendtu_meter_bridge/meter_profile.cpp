// SPDX-License-Identifier: Apache-2.0

#include "meter_profile.h"

#include <cstring>

namespace esphome::opendtu_meter_bridge {

static constexpr uint16_t SDM630_REGISTER_START = 0x0000;
static constexpr uint16_t SDM630_REGISTER_COUNT = 0x0180;
static constexpr uint16_t DTSU666_REGISTER_START = 0x2000;
static constexpr uint16_t DTSU666_REGISTER_COUNT = 0x0046;

uint16_t meter_profile_register_start(MeterProfile profile) {
  return profile == MeterProfile::DTSU666 ? DTSU666_REGISTER_START : SDM630_REGISTER_START;
}

uint16_t meter_profile_register_count(MeterProfile profile) {
  return profile == MeterProfile::DTSU666 ? DTSU666_REGISTER_COUNT : SDM630_REGISTER_COUNT;
}

const char *meter_profile_name(MeterProfile profile) {
  return profile == MeterProfile::DTSU666 ? "DTSU666" : "SDM630";
}

void encode_meter_profile(MeterProfile profile, const MeterMeasurements &measurements, uint16_t *registers,
                          size_t register_capacity) {
  memset(registers, 0, register_capacity * sizeof(uint16_t));
  if (profile == MeterProfile::DTSU666) {
    profiles::write_dtsu666_profile(measurements, registers, register_capacity);
    return;
  }
  profiles::write_sdm630_profile(measurements, registers, register_capacity);
}

}  // namespace esphome::opendtu_meter_bridge
