// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "meter_profile.h"

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/macros.h"
#include "esphome/core/version.h"
#include "esphome/components/modbus/modbus.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#include <cJSON.h>
#include <esp_websocket_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstdint>
#include <string>
#include <vector>

namespace esphome::opendtu_meter_bridge {

struct MicroinverterMapEntry {
  std::string inverter_serial;
  std::string inverter_name;
  uint8_t grid_phase;
};

struct PhaseData {
  float voltage{0.0f};
  float current{0.0f};
  float power{0.0f};
  bool has_data{false};
};

class OpenDtuMeterBridge;

class MeterBridgeModbusServer
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 6, 0)
    : public modbus::ModbusServerDevice {
#else
    : public modbus::ModbusDevice {
#endif
 public:
  void set_bridge(OpenDtuMeterBridge *bridge) { this->bridge_ = bridge; }
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 6, 0)
  modbus::ResponseStatus on_read_registers(uint16_t start_address, uint16_t number_of_registers,
                                           modbus::RegisterValues &registers) override;
#else
  void on_modbus_data(const std::vector<uint8_t> &data) override {}
  void on_modbus_read_registers(uint8_t function_code, uint16_t start_address,
                                uint16_t number_of_registers) override;
#endif

 protected:
  OpenDtuMeterBridge *bridge_{nullptr};
};

#if ESPHOME_VERSION_CODE < VERSION_CODE(2026, 6, 0)
// Swallows frames at Deye's main-meter address 0x01 to suppress unknown-address logs.
class ModbusSilenceDevice : public modbus::ModbusDevice {
 public:
  void on_modbus_data(const std::vector<uint8_t> &data) override {}
};
#endif

class OpenDtuMeterBridge : public Component
#ifdef USE_WIFI_LISTENERS
    ,
                      public wifi::WiFiConnectStateListener
#endif
{
 public:
  void set_host(const std::string &host) { this->host_ = host; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_path(const std::string &path) { this->path_ = path; }
  void set_username(const std::string &username) { this->username_ = username; }
  void set_password(const std::string &password) { this->password_ = password; }
  void set_data_timeout_ms(uint32_t ms) { this->data_timeout_ms_ = ms; }
  void set_default_voltage(float value) { this->default_voltage_ = value; }
  void set_default_frequency(float value) { this->default_frequency_ = value; }
  void set_meter_profile(MeterProfile profile) { this->meter_profile_ = profile; }
  void set_component_version(const std::string &version) { this->component_version_ = version; }
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 6, 0)
  void set_modbus_server(modbus::ModbusServerHub *parent, uint8_t slave_address);
#else
  void set_modbus_server(modbus::Modbus *parent, uint8_t slave_address);
#endif
  void add_microinverter_map_by_serial(const std::string &inverter_serial, uint8_t grid_phase);
  void add_microinverter_map_by_name(const std::string &inverter_name, uint8_t grid_phase);

  float get_voltage(int phase);
  float get_current(int phase);
  float get_power(int phase);
  float get_total_power();
  float get_frequency();
  bool is_data_valid();
  bool is_websocket_connected() const { return this->ws_connected_; }
  bool read_modbus_registers(uint16_t start_address, uint16_t number_of_registers,
                             std::vector<uint8_t> &response);

#ifdef USE_SENSOR
  void set_voltage_l1_sensor(sensor::Sensor *sensor) { this->voltage_l1_sensor_ = sensor; }
  void set_voltage_l2_sensor(sensor::Sensor *sensor) { this->voltage_l2_sensor_ = sensor; }
  void set_voltage_l3_sensor(sensor::Sensor *sensor) { this->voltage_l3_sensor_ = sensor; }
  void set_current_l1_sensor(sensor::Sensor *sensor) { this->current_l1_sensor_ = sensor; }
  void set_current_l2_sensor(sensor::Sensor *sensor) { this->current_l2_sensor_ = sensor; }
  void set_current_l3_sensor(sensor::Sensor *sensor) { this->current_l3_sensor_ = sensor; }
  void set_power_l1_sensor(sensor::Sensor *sensor) { this->power_l1_sensor_ = sensor; }
  void set_power_l2_sensor(sensor::Sensor *sensor) { this->power_l2_sensor_ = sensor; }
  void set_power_l3_sensor(sensor::Sensor *sensor) { this->power_l3_sensor_ = sensor; }
  void set_total_power_sensor(sensor::Sensor *sensor) { this->total_power_sensor_ = sensor; }
  void set_frequency_sensor(sensor::Sensor *sensor) { this->frequency_sensor_ = sensor; }
#endif
#ifdef USE_BINARY_SENSOR
  void set_websocket_connected_binary_sensor(binary_sensor::BinarySensor *sensor) {
    this->websocket_connected_binary_sensor_ = sensor;
  }
  void set_data_valid_binary_sensor(binary_sensor::BinarySensor *sensor) {
    this->data_valid_binary_sensor_ = sensor;
  }
#endif
#ifdef USE_TEXT_SENSOR
  void set_component_version_text_sensor(text_sensor::TextSensor *sensor) {
    this->component_version_text_sensor_ = sensor;
  }
#endif

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

#ifdef USE_WIFI_LISTENERS
  void on_wifi_connect_state(const std::string &ssid, const wifi::bssid_t &bssid) override;
#endif

 protected:
  static bool float_is_finite(float value);
  float modbus_voltage(float measured, bool has_phase_data);
  float modbus_current_power(float measured, bool has_phase_data);
  float modbus_frequency(float measured, bool has_frequency_data);

  void mark_data_stale_and_reset_();
  void clear_phase_measurements_();
  void sync_modbus_registers_();
  void write_modbus_defaults_();
  void process_livedata_(const char *json, size_t len);
  void publish_state_();
  float json_field_v_(const cJSON *ac0, const char *key);
  int find_inverter_index_(const cJSON *inverters, const MicroinverterMapEntry &entry);

  bool ws_buf_ensure_(size_t needed);
  void start_websocket_();
  void stop_websocket_();
  static void websocket_event_handler_(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

  std::string host_;
  std::string path_{"/livedata"};
  std::string username_;
  std::string password_;
  uint16_t port_{80};
  uint32_t data_timeout_ms_{15000};
  float default_voltage_{230.0f};
  float default_frequency_{50.0f};
  MeterProfile meter_profile_{MeterProfile::SDM630};
  std::string component_version_;

  std::vector<MicroinverterMapEntry> microinverter_map_;
  PhaseData phase_[4];
  float measured_frequency_{0.0f};
  bool has_frequency_data_{false};
  uint16_t modbus_regs_[METER_PROFILE_REGISTER_CAPACITY]{};
  SemaphoreHandle_t data_mutex_{nullptr};

  char *ws_buf_{nullptr};
  size_t ws_cap_{0};
  esp_websocket_client_handle_t ws_client_{nullptr};
  bool ws_started_{false};
  bool ws_connected_{false};
  volatile int64_t last_data_us_{0};
  volatile bool data_stale_{true};

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 6, 0)
  modbus::ModbusServerHub *modbus_parent_{nullptr};
#else
  modbus::Modbus *modbus_parent_{nullptr};
#endif
  uint8_t modbus_slave_address_{0};
  MeterBridgeModbusServer modbus_server_device_{};
#if ESPHOME_VERSION_CODE < VERSION_CODE(2026, 6, 0)
  ModbusSilenceDevice modbus_silence_device_{};
#endif

#ifdef USE_SENSOR
  sensor::Sensor *voltage_l1_sensor_{nullptr};
  sensor::Sensor *voltage_l2_sensor_{nullptr};
  sensor::Sensor *voltage_l3_sensor_{nullptr};
  sensor::Sensor *current_l1_sensor_{nullptr};
  sensor::Sensor *current_l2_sensor_{nullptr};
  sensor::Sensor *current_l3_sensor_{nullptr};
  sensor::Sensor *power_l1_sensor_{nullptr};
  sensor::Sensor *power_l2_sensor_{nullptr};
  sensor::Sensor *power_l3_sensor_{nullptr};
  sensor::Sensor *total_power_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
#endif
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *websocket_connected_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *data_valid_binary_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *component_version_text_sensor_{nullptr};
#endif
};

#ifdef USE_BUTTON
class RebootDeviceButton : public button::Button, public Component {
 public:
  void dump_config() override;

 protected:
  void press_action() override;
};
#endif

}  // namespace esphome::opendtu_meter_bridge
