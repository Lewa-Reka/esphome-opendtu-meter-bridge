// SPDX-License-Identifier: Apache-2.0

#include "components/opendtu_meter_bridge/meter_profile.h"
#include "components/opendtu_meter_bridge/meter_profile_codec.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace meter = esphome::opendtu_meter_bridge;

static int failures = 0;

static void fail(const char *message) {
  std::fprintf(stderr, "FAIL: %s\n", message);
  failures++;
}

static void expect_true(bool condition, const char *message) {
  if (!condition) {
    fail(message);
  }
}

static void expect_word(uint16_t actual, uint16_t expected, const char *message) {
  if (actual != expected) {
    std::fprintf(stderr, "FAIL: %s (actual=0x%04X, expected=0x%04X)\n", message, actual, expected);
    failures++;
  }
}

static void expect_float(float actual, float expected, float tolerance, const char *message) {
  if (!std::isfinite(actual) || std::fabs(actual - expected) > tolerance) {
    std::fprintf(stderr, "FAIL: %s (actual=%.6f, expected=%.6f)\n", message, actual, expected);
    failures++;
  }
}

static float decode_float_abcd(const uint16_t *registers, size_t offset) {
  const uint32_t bits = (static_cast<uint32_t>(registers[offset]) << 16) | registers[offset + 1];
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

static bool range_contains(meter::MeterProfile profile, uint16_t address, uint16_t count) {
  const uint32_t start = meter::meter_profile_register_start(profile);
  const uint32_t end = start + meter::meter_profile_register_count(profile);
  const uint32_t requested_end = static_cast<uint32_t>(address) + count;
  return count > 0 && address >= start && requested_end <= end;
}

static meter::MeterMeasurements sample_measurements() {
  meter::MeterMeasurements measurements;
  measurements.voltage[1] = 230.0f;
  measurements.voltage[2] = 231.0f;
  measurements.voltage[3] = 232.0f;
  measurements.current[1] = -1.5f;
  measurements.current[2] = -2.25f;
  measurements.current[3] = -3.75f;
  measurements.power[1] = -1000.0f;
  measurements.power[2] = -2000.0f;
  measurements.power[3] = -3000.0f;
  measurements.total_power = -6000.0f;
  measurements.total_reactive_power = -100.0f;
  measurements.frequency = 50.0f;
  return measurements;
}

static void test_profile_ranges() {
  expect_word(meter::meter_profile_register_start(meter::MeterProfile::SDM630), 0x0000,
              "SDM630 register window starts at 0x0000");
  expect_word(meter::meter_profile_register_count(meter::MeterProfile::SDM630), 0x0180,
              "SDM630 register window has the expected size");
  expect_word(meter::meter_profile_register_start(meter::MeterProfile::DTSU666), 0x2000,
              "DTSU666 instantaneous register window starts at 0x2000");
  expect_word(meter::meter_profile_register_count(meter::MeterProfile::DTSU666), 0x0046,
              "DTSU666 instantaneous register window ends after 0x2045");

  expect_true(range_contains(meter::MeterProfile::DTSU666, 0x2000, 0x0046),
              "The complete DTSU666 instantaneous block is readable");
  expect_true(range_contains(meter::MeterProfile::DTSU666, 0x2044, 2),
              "The DTSU666 frequency float fits at the upper boundary");
  expect_true(!range_contains(meter::MeterProfile::DTSU666, 0x2044, 3),
              "A DTSU666 request beyond the upper boundary is rejected");
  expect_true(!range_contains(meter::MeterProfile::DTSU666, 0x101E, 2),
              "Unimplemented DTSU666 cumulative energy is outside the exposed window");
  expect_true(!range_contains(meter::MeterProfile::DTSU666, 0x0000, 1),
              "Unimplemented DTSU666 configuration is outside the exposed window");
}

static void test_sdm630_golden_vector() {
  std::array<uint16_t, meter::METER_PROFILE_REGISTER_CAPACITY> registers;
  registers.fill(0xA5A5);
  const auto measurements = sample_measurements();
  meter::encode_meter_profile(meter::MeterProfile::SDM630, measurements, registers.data(), registers.size());

  expect_word(registers[0x0000], 0x4366, "SDM630 230 V high word is IEEE-754 ABCD");
  expect_word(registers[0x0001], 0x0000, "SDM630 230 V low word is IEEE-754 ABCD");
  expect_word(registers[0x0006], 0xBFC0, "SDM630 preserves the existing directional current sign");
  expect_word(registers[0x0007], 0x0000, "SDM630 directional current low word matches the golden vector");
  expect_word(registers[0x000C], 0xC47A, "SDM630 export power remains negative");
  expect_word(registers[0x000D], 0x0000, "SDM630 export power low word matches the golden vector");
  expect_word(registers[0x0034], 0xC5BB, "SDM630 total export power remains negative");
  expect_word(registers[0x0035], 0x8000, "SDM630 total power low word matches the golden vector");
  expect_word(registers[0x0046], 0x4248, "SDM630 50 Hz high word matches the golden vector");
  expect_word(registers[0x0047], 0x0000, "SDM630 50 Hz low word matches the golden vector");
  expect_word(registers[0x0012], 0x0000, "Unimplemented SDM630 registers are cleared to zero");

  expect_float(decode_float_abcd(registers.data(), 0x0002), 231.0f, 0.0001f,
               "SDM630 L2 voltage uses address 0x0002");
  expect_float(decode_float_abcd(registers.data(), 0x0004), 232.0f, 0.0001f,
               "SDM630 L3 voltage uses address 0x0004");
  expect_float(decode_float_abcd(registers.data(), 0x0008), -2.25f, 0.0001f,
               "SDM630 L2 current uses address 0x0008");
  expect_float(decode_float_abcd(registers.data(), 0x000A), -3.75f, 0.0001f,
               "SDM630 L3 current uses address 0x000A");
  expect_float(decode_float_abcd(registers.data(), 0x000E), -2000.0f, 0.0001f,
               "SDM630 L2 power uses address 0x000E");
  expect_float(decode_float_abcd(registers.data(), 0x0010), -3000.0f, 0.0001f,
               "SDM630 L3 power uses address 0x0010");
}

static void test_dtsu666_golden_vector() {
  std::array<uint16_t, meter::METER_PROFILE_REGISTER_CAPACITY> registers;
  registers.fill(0xA5A5);
  const auto measurements = sample_measurements();
  meter::encode_meter_profile(meter::MeterProfile::DTSU666, measurements, registers.data(), registers.size());

  expect_float(decode_float_abcd(registers.data(), 0x0006), 2300.0f, 0.0001f,
               "DTSU666 Ua uses the documented 0.1 V scale");
  expect_float(decode_float_abcd(registers.data(), 0x000C), 1500.0f, 0.0001f,
               "DTSU666 Ia uses the documented 0.001 A scale and RMS magnitude");
  expect_float(decode_float_abcd(registers.data(), 0x0012), -60000.0f, 0.0001f,
               "DTSU666 total active power uses the documented 0.1 W scale");
  expect_float(decode_float_abcd(registers.data(), 0x0014), -10000.0f, 0.0001f,
               "DTSU666 phase active power uses the documented 0.1 W scale");
  expect_float(decode_float_abcd(registers.data(), 0x001A), -1000.0f, 0.0001f,
               "DTSU666 Qt uses address 0x201A and the documented 0.1 var scale");
  expect_float(decode_float_abcd(registers.data(), 0x0044), 5000.0f, 0.0001f,
               "DTSU666 frequency uses the documented 0.01 Hz scale");

  expect_float(decode_float_abcd(registers.data(), 0x000E), 2250.0f, 0.0001f,
               "DTSU666 Ib uses the documented 0.001 A scale");
  expect_float(decode_float_abcd(registers.data(), 0x0010), 3750.0f, 0.0001f,
               "DTSU666 Ic uses the documented 0.001 A scale");
  expect_float(decode_float_abcd(registers.data(), 0x0008), 2310.0f, 0.0001f,
               "DTSU666 Ub uses address 0x2008 and the documented 0.1 V scale");
  expect_float(decode_float_abcd(registers.data(), 0x000A), 2320.0f, 0.0001f,
               "DTSU666 Uc uses address 0x200A and the documented 0.1 V scale");
  expect_float(decode_float_abcd(registers.data(), 0x0016), -20000.0f, 0.0001f,
               "DTSU666 Pb uses address 0x2016 and the documented 0.1 W scale");
  expect_float(decode_float_abcd(registers.data(), 0x0018), -30000.0f, 0.0001f,
               "DTSU666 Pc uses address 0x2018 and the documented 0.1 W scale");
}

static void test_dtsu666_line_voltage_formula() {
  std::array<uint16_t, meter::METER_PROFILE_REGISTER_CAPACITY> registers{};
  const auto measurements = sample_measurements();
  meter::encode_meter_profile(meter::MeterProfile::DTSU666, measurements, registers.data(), registers.size());

  const float expected_ab = std::sqrt(230.0f * 230.0f + 231.0f * 231.0f + 230.0f * 231.0f);
  const float expected_bc = std::sqrt(231.0f * 231.0f + 232.0f * 232.0f + 231.0f * 232.0f);
  const float expected_ca = std::sqrt(232.0f * 232.0f + 230.0f * 230.0f + 232.0f * 230.0f);

  expect_float(decode_float_abcd(registers.data(), 0x0000), expected_ab * 10.0f, 0.0001f,
               "DTSU666 Uab uses the 120-degree phase formula");
  expect_float(decode_float_abcd(registers.data(), 0x0002), expected_bc * 10.0f, 0.0001f,
               "DTSU666 Ubc uses the 120-degree phase formula");
  expect_float(decode_float_abcd(registers.data(), 0x0004), expected_ca * 10.0f, 0.0001f,
               "DTSU666 Uca uses the 120-degree phase formula");
}

static void test_descriptor_bounds() {
  using esphome::opendtu_meter_bridge::profiles::internal::FloatRegisterDescriptor;
  using esphome::opendtu_meter_bridge::profiles::internal::MeasurementSource;
  using esphome::opendtu_meter_bridge::profiles::internal::write_float_registers;

  struct GuardedRegisters {
    uint16_t before;
    uint16_t values[2];
    uint16_t after;
  } guarded{0x1111, {0x2222, 0x3333}, 0x4444};

  const FloatRegisterDescriptor descriptors[] = {
      {0x1FFE, MeasurementSource::VOLTAGE_L1, 1.0f},
      {0x2001, MeasurementSource::VOLTAGE_L1, 1.0f},
      {0x2002, MeasurementSource::FREQUENCY, 1.0f},
  };
  const auto measurements = sample_measurements();
  write_float_registers(0x2000, descriptors, sizeof(descriptors) / sizeof(descriptors[0]), measurements,
                        guarded.values, 1);

  expect_word(guarded.before, 0x1111, "A descriptor below the register window cannot underflow the buffer");
  expect_word(guarded.values[0], 0x2222, "An incomplete FP32 destination is not partially written");
  expect_word(guarded.values[1], 0x3333, "The capacity boundary is preserved");
  expect_word(guarded.after, 0x4444, "A descriptor beyond the register window cannot overflow the buffer");

  std::array<uint16_t, 70> complete_window{};
  meter::profiles::write_dtsu666_profile(measurements, complete_window.data(), complete_window.size());
  expect_float(decode_float_abcd(complete_window.data(), 68), 5000.0f, 0.0001f,
               "DTSU666 scaled frequency fits in a 70-register buffer");

  std::array<uint16_t, 69> short_window;
  short_window.fill(0xA5A5);
  meter::profiles::write_dtsu666_profile(measurements, short_window.data(), short_window.size());
  expect_word(short_window[68], 0xA5A5, "DTSU666 frequency is skipped when only one destination word remains");
}

static void test_empty_destinations() {
  const auto measurements = sample_measurements();
  meter::encode_meter_profile(meter::MeterProfile::SDM630, measurements, nullptr, 0);
  meter::profiles::write_dtsu666_profile(measurements, nullptr, 0);
}

int main() {
  test_profile_ranges();
  test_sdm630_golden_vector();
  test_dtsu666_golden_vector();
  test_dtsu666_line_voltage_formula();
  test_descriptor_bounds();
  test_empty_destinations();

  if (failures != 0) {
    std::fprintf(stderr, "%d meter profile test(s) failed\n", failures);
    return 1;
  }

  std::puts("All meter profile tests passed");
  return 0;
}
