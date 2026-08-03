// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::opendtu_meter_bridge {

enum class MeterProfile : uint8_t {
  SDM630,
  DTSU666,
};

struct MeterMeasurements {
  float voltage[4]{};
  float current[4]{};
  float power[4]{};
  float total_power{0.0f};
  float frequency{0.0f};
};

static constexpr size_t METER_PROFILE_REGISTER_CAPACITY = 0x0180;

uint16_t meter_profile_register_start(MeterProfile profile);
uint16_t meter_profile_register_count(MeterProfile profile);
const char *meter_profile_name(MeterProfile profile);
void encode_meter_profile(MeterProfile profile, const MeterMeasurements &measurements, uint16_t *registers,
                          size_t register_capacity);

namespace profiles {

void write_sdm630_profile(const MeterMeasurements &measurements, uint16_t *registers, size_t register_capacity);
void write_dtsu666_profile(const MeterMeasurements &measurements, uint16_t *registers, size_t register_capacity);

}  // namespace profiles
}  // namespace esphome::opendtu_meter_bridge
