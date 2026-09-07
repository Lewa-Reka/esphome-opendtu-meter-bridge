// SPDX-License-Identifier: Apache-2.0

#include "meter_profile.h"
#include "meter_profile_codec.h"

namespace esphome::opendtu_meter_bridge::profiles {

static constexpr internal::FloatRegisterDescriptor REGISTER_MAP[] = {
    {0x0000, internal::MeasurementSource::VOLTAGE_L1},
    {0x0002, internal::MeasurementSource::VOLTAGE_L2},
    {0x0004, internal::MeasurementSource::VOLTAGE_L3},
    {0x0006, internal::MeasurementSource::DIRECTIONAL_CURRENT_L1},
    {0x0008, internal::MeasurementSource::DIRECTIONAL_CURRENT_L2},
    {0x000A, internal::MeasurementSource::DIRECTIONAL_CURRENT_L3},
    {0x000C, internal::MeasurementSource::POWER_L1},
    {0x000E, internal::MeasurementSource::POWER_L2},
    {0x0010, internal::MeasurementSource::POWER_L3},
    {0x0034, internal::MeasurementSource::TOTAL_POWER},
    {0x0046, internal::MeasurementSource::FREQUENCY},
};

void write_sdm630_profile(const MeterMeasurements &measurements, uint16_t *registers, size_t register_capacity) {
  internal::write_float_registers(meter_profile_register_start(MeterProfile::SDM630), REGISTER_MAP,
                                  sizeof(REGISTER_MAP) / sizeof(REGISTER_MAP[0]), measurements, registers,
                                  register_capacity);
}

}  // namespace esphome::opendtu_meter_bridge::profiles
