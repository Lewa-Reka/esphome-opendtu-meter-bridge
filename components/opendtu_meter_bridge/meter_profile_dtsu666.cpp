// SPDX-License-Identifier: Apache-2.0

#include "meter_profile.h"
#include "meter_profile_codec.h"

namespace esphome::opendtu_meter_bridge::profiles {

static constexpr internal::FloatRegisterDescriptor REGISTER_MAP[] = {
    {0x2000, internal::MeasurementSource::LINE_VOLTAGE_AB},
    {0x2002, internal::MeasurementSource::LINE_VOLTAGE_BC},
    {0x2004, internal::MeasurementSource::LINE_VOLTAGE_CA},
    {0x2006, internal::MeasurementSource::VOLTAGE_L1},
    {0x2008, internal::MeasurementSource::VOLTAGE_L2},
    {0x200A, internal::MeasurementSource::VOLTAGE_L3},
    {0x200C, internal::MeasurementSource::CURRENT_RMS_L1},
    {0x200E, internal::MeasurementSource::CURRENT_RMS_L2},
    {0x2010, internal::MeasurementSource::CURRENT_RMS_L3},
    {0x2012, internal::MeasurementSource::TOTAL_POWER},
    {0x2014, internal::MeasurementSource::POWER_L1},
    {0x2016, internal::MeasurementSource::POWER_L2},
    {0x2018, internal::MeasurementSource::POWER_L3},
    {0x2044, internal::MeasurementSource::FREQUENCY},
};

void write_dtsu666_profile(const MeterMeasurements &measurements, uint16_t *registers, size_t register_capacity) {
  internal::write_float_registers(meter_profile_register_start(MeterProfile::DTSU666), REGISTER_MAP,
                                  sizeof(REGISTER_MAP) / sizeof(REGISTER_MAP[0]), measurements, registers,
                                  register_capacity);
}

}  // namespace esphome::opendtu_meter_bridge::profiles
