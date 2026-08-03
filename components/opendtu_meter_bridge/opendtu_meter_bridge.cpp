// SPDX-License-Identifier: Apache-2.0

#include "opendtu_meter_bridge.h"

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

#include "esp_timer.h"
#include "mbedtls/base64.h"

namespace esphome::opendtu_meter_bridge {

static const char *const TAG = "opendtu_meter_bridge";

static constexpr uint8_t DEYE_MAIN_METER_ADDRESS = 0x01;
static constexpr uint16_t MAX_MODBUS_READ_REGISTERS = 125;
static constexpr size_t MAX_PENDING_WEBSOCKET_EVENTS = 8;

static constexpr float VOLTAGE_MIN_V = 100.0f;
static constexpr float VOLTAGE_MAX_V = 300.0f;
static constexpr float FREQUENCY_MIN_HZ = 40.0f;
static constexpr float FREQUENCY_MAX_HZ = 65.0f;

struct PhaseAccumulator {
  float voltage_sum{0.0f};
  uint8_t voltage_count{0};
  float current{0.0f};
  float power{0.0f};
  bool has_data{false};
};

void OpenDtuMeterBridge::add_microinverter_map_by_serial(const std::string &inverter_serial, uint8_t grid_phase) {
  this->microinverter_map_.push_back({inverter_serial, "", grid_phase});
}

void OpenDtuMeterBridge::add_microinverter_map_by_name(const std::string &inverter_name, uint8_t grid_phase) {
  this->microinverter_map_.push_back({"", inverter_name, grid_phase});
}

bool OpenDtuMeterBridge::float_is_finite(float value) { return std::isfinite(value); }

float OpenDtuMeterBridge::modbus_voltage(float measured, bool has_phase_data) {
  if (!has_phase_data || !float_is_finite(measured) || measured < VOLTAGE_MIN_V || measured > VOLTAGE_MAX_V) {
    return this->default_voltage_;
  }
  return measured;
}

float OpenDtuMeterBridge::modbus_current_power(float measured, bool has_phase_data) {
  if (!has_phase_data || !float_is_finite(measured)) {
    return 0.0f;
  }
  return measured;
}

float OpenDtuMeterBridge::modbus_frequency(float measured, bool has_frequency_data) {
  if (!has_frequency_data || !float_is_finite(measured) || measured < FREQUENCY_MIN_HZ || measured > FREQUENCY_MAX_HZ) {
    return this->default_frequency_;
  }
  return measured;
}

void OpenDtuMeterBridge::write_modbus_defaults_() {
  MeterMeasurements measurements;
  measurements.voltage[1] = this->default_voltage_;
  measurements.voltage[2] = this->default_voltage_;
  measurements.voltage[3] = this->default_voltage_;
  measurements.frequency = this->default_frequency_;
  encode_meter_profile(this->meter_profile_, measurements, this->modbus_regs_, METER_PROFILE_REGISTER_CAPACITY);
}

void OpenDtuMeterBridge::sync_modbus_registers_locked_() {
  if (this->data_stale_) {
    this->write_modbus_defaults_();
  } else {
    MeterMeasurements measurements;
    for (int ph = 1; ph <= 3; ph++) {
      bool has = this->phase_[ph].has_data;
      measurements.voltage[ph] = this->modbus_voltage(this->phase_[ph].voltage, has);
      measurements.current[ph] = this->modbus_current_power(this->phase_[ph].current, has);
      measurements.power[ph] = this->modbus_current_power(this->phase_[ph].power, has);
      measurements.total_power += measurements.power[ph];
    }
    if (!float_is_finite(measurements.total_power)) {
      measurements.total_power = 0.0f;
    }
    measurements.frequency = this->modbus_frequency(this->measured_frequency_, this->has_frequency_data_);
    encode_meter_profile(this->meter_profile_, measurements, this->modbus_regs_, METER_PROFILE_REGISTER_CAPACITY);
  }
}

void OpenDtuMeterBridge::sync_modbus_registers_() {
  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  this->sync_modbus_registers_locked_();
  xSemaphoreGive(this->data_mutex_);
}

bool OpenDtuMeterBridge::read_modbus_registers(uint16_t start_address, uint16_t number_of_registers,
                                               std::vector<uint8_t> &response) {
  const uint32_t window_start = meter_profile_register_start(this->meter_profile_);
  const uint32_t window_end = window_start + meter_profile_register_count(this->meter_profile_);
  const uint32_t request_end = (uint32_t) start_address + number_of_registers;
  if (number_of_registers == 0 || number_of_registers > MAX_MODBUS_READ_REGISTERS ||
      start_address < window_start || request_end > window_end) {
    return false;
  }

  response.clear();
  response.reserve((size_t) number_of_registers * 2u);
  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  for (uint16_t offset = 0; offset < number_of_registers; offset++) {
    const uint16_t index = (uint16_t) ((uint32_t) start_address + offset - window_start);
    const uint16_t value = this->modbus_regs_[index];
    response.push_back((uint8_t) (value >> 8));
    response.push_back((uint8_t) (value & 0xFFu));
  }
  xSemaphoreGive(this->data_mutex_);
  return true;
}

void OpenDtuMeterBridge::mark_data_stale_and_reset_() {
  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  this->data_stale_ = true;
  for (auto &phase : this->phase_) {
    phase = PhaseData{};
  }
  this->measured_frequency_ = 0.0f;
  this->has_frequency_data_ = false;
  this->sync_modbus_registers_locked_();
  xSemaphoreGive(this->data_mutex_);
  this->publish_state_();
}

void OpenDtuMeterBridge::publish_state_() {
#ifdef USE_SENSOR
  if (this->voltage_l1_sensor_ != nullptr) {
    this->voltage_l1_sensor_->publish_state(this->get_voltage(1));
  }
  if (this->voltage_l2_sensor_ != nullptr) {
    this->voltage_l2_sensor_->publish_state(this->get_voltage(2));
  }
  if (this->voltage_l3_sensor_ != nullptr) {
    this->voltage_l3_sensor_->publish_state(this->get_voltage(3));
  }
  if (this->current_l1_sensor_ != nullptr) {
    this->current_l1_sensor_->publish_state(this->get_current(1));
  }
  if (this->current_l2_sensor_ != nullptr) {
    this->current_l2_sensor_->publish_state(this->get_current(2));
  }
  if (this->current_l3_sensor_ != nullptr) {
    this->current_l3_sensor_->publish_state(this->get_current(3));
  }
  if (this->power_l1_sensor_ != nullptr) {
    this->power_l1_sensor_->publish_state(this->get_power(1));
  }
  if (this->power_l2_sensor_ != nullptr) {
    this->power_l2_sensor_->publish_state(this->get_power(2));
  }
  if (this->power_l3_sensor_ != nullptr) {
    this->power_l3_sensor_->publish_state(this->get_power(3));
  }
  if (this->total_power_sensor_ != nullptr) {
    this->total_power_sensor_->publish_state(this->get_total_power());
  }
  if (this->frequency_sensor_ != nullptr) {
    this->frequency_sensor_->publish_state(this->get_frequency());
  }
#endif
#ifdef USE_BINARY_SENSOR
  if (this->websocket_connected_binary_sensor_ != nullptr) {
    this->websocket_connected_binary_sensor_->publish_state(this->is_websocket_connected());
  }
  if (this->data_valid_binary_sensor_ != nullptr) {
    this->data_valid_binary_sensor_->publish_state(this->is_data_valid());
  }
#endif
}

float OpenDtuMeterBridge::json_field_v_(const cJSON *ac0, const char *key) {
  const cJSON *field = cJSON_GetObjectItemCaseSensitive(ac0, key);
  if (field == nullptr) {
    return 0.0f;
  }
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(field, "v");
  if (cJSON_IsNumber(v)) {
    float out = (float) v->valuedouble;
    return float_is_finite(out) ? out : 0.0f;
  }
  return 0.0f;
}

int OpenDtuMeterBridge::find_inverter_index_(const cJSON *inverters, const MicroinverterMapEntry &entry) {
  const int count = cJSON_GetArraySize(inverters);
  const char *field = !entry.inverter_serial.empty() ? "serial" : "name";
  const std::string &match = !entry.inverter_serial.empty() ? entry.inverter_serial : entry.inverter_name;

  for (int i = 0; i < count; i++) {
    cJSON *inv = cJSON_GetArrayItem(inverters, i);
    if (inv == nullptr) {
      continue;
    }
    cJSON *value = cJSON_GetObjectItemCaseSensitive(inv, field);
    if (!cJSON_IsString(value) || value->valuestring == nullptr) {
      continue;
    }
    if (match == value->valuestring) {
      return i;
    }
  }
  return -1;
}

void OpenDtuMeterBridge::process_livedata_(const char *json, size_t len) {
  cJSON *root = cJSON_ParseWithLength(json, len);
  if (root == nullptr) {
    ESP_LOGW(TAG, "Failed to parse livedata JSON (length=%u bytes)", (unsigned) len);
    this->mark_data_stale_and_reset_();
    return;
  }

  cJSON *inverters = cJSON_GetObjectItemCaseSensitive(root, "inverters");
  if (!cJSON_IsArray(inverters)) {
    ESP_LOGW(TAG, "Missing 'inverters' array in livedata payload");
    cJSON_Delete(root);
    this->mark_data_stale_and_reset_();
    return;
  }

  PhaseAccumulator tmp[4] = {};
  float frequency_sum = 0.0f;
  uint8_t frequency_count = 0;

  for (const auto &entry : this->microinverter_map_) {
    uint8_t grid_phase = entry.grid_phase;
    if (grid_phase < 1 || grid_phase > 3) {
      continue;
    }

    int idx = this->find_inverter_index_(inverters, entry);
    if (idx < 0) {
      if (!entry.inverter_serial.empty()) {
        ESP_LOGW(TAG, "Microinverter serial '%s' not found in livedata", entry.inverter_serial.c_str());
      } else {
        ESP_LOGW(TAG, "Microinverter '%s' not found in livedata", entry.inverter_name.c_str());
      }
      continue;
    }

    cJSON *inv = cJSON_GetArrayItem(inverters, idx);
    if (inv == nullptr) {
      continue;
    }

    cJSON *ac = cJSON_GetObjectItemCaseSensitive(inv, "AC");
    cJSON *ac0 = (ac != nullptr) ? cJSON_GetObjectItemCaseSensitive(ac, "0") : nullptr;
    if (ac0 == nullptr) {
      continue;
    }

    float voltage = this->json_field_v_(ac0, "Voltage");
    float current = this->json_field_v_(ac0, "Current");
    float power = this->json_field_v_(ac0, "Power");
    float frequency = this->json_field_v_(ac0, "Frequency");

    if (float_is_finite(voltage) && voltage >= VOLTAGE_MIN_V && voltage <= VOLTAGE_MAX_V) {
      tmp[grid_phase].voltage_sum += voltage;
      tmp[grid_phase].voltage_count++;
    }
    tmp[grid_phase].current += -current;
    tmp[grid_phase].power += -power;
    tmp[grid_phase].has_data = true;

    if (float_is_finite(frequency) && frequency >= FREQUENCY_MIN_HZ && frequency <= FREQUENCY_MAX_HZ) {
      frequency_sum += frequency;
      frequency_count++;
    }
  }

  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  for (int ph = 1; ph <= 3; ph++) {
    this->phase_[ph].current = tmp[ph].current;
    this->phase_[ph].power = tmp[ph].power;
    this->phase_[ph].has_data = tmp[ph].has_data;
    if (tmp[ph].voltage_count > 0) {
      this->phase_[ph].voltage = tmp[ph].voltage_sum / (float) tmp[ph].voltage_count;
    } else {
      this->phase_[ph].voltage = 0.0f;
    }
  }
  if (frequency_count > 0) {
    this->measured_frequency_ = frequency_sum / (float) frequency_count;
    this->has_frequency_data_ = true;
  } else {
    this->measured_frequency_ = 0.0f;
    this->has_frequency_data_ = false;
  }
  this->last_data_us_ = esp_timer_get_time();
  this->data_stale_ = false;
  this->sync_modbus_registers_locked_();
  xSemaphoreGive(this->data_mutex_);

  cJSON_Delete(root);

  float v1 = this->get_voltage(1);
  float c1 = this->get_current(1);
  float p1 = this->get_power(1);
  float v2 = this->get_voltage(2);
  float c2 = this->get_current(2);
  float p2 = this->get_power(2);
  float v3 = this->get_voltage(3);
  float c3 = this->get_current(3);
  float p3 = this->get_power(3);
  float total = this->get_total_power();

  ESP_LOGI(TAG, "L1: %.1f V, %.2f A, %.1f W", v1, c1, p1);
  ESP_LOGI(TAG, "L2: %.1f V, %.2f A, %.1f W", v2, c2, p2);
  ESP_LOGI(TAG, "L3: %.1f V, %.2f A, %.1f W", v3, c3, p3);
  ESP_LOGI(TAG, "Total Power: %.1f W", total);
  ESP_LOGI(TAG, "Frequency: %.2f Hz", this->get_frequency());

  this->publish_state_();
}

bool OpenDtuMeterBridge::ws_buf_ensure_(size_t needed) {
  if (needed <= this->ws_cap_) {
    return true;
  }
  size_t new_cap = (this->ws_cap_ == 0) ? 4096 : this->ws_cap_;
  while (new_cap < needed) {
    new_cap *= 2;
  }
  char *p = (char *) realloc(this->ws_buf_, new_cap);
  if (p == nullptr) {
    ESP_LOGE(TAG, "Out of memory for WebSocket buffer (requested %u bytes)", (unsigned) new_cap);
    return false;
  }
  this->ws_buf_ = p;
  this->ws_cap_ = new_cap;
  return true;
}

void OpenDtuMeterBridge::queue_websocket_event_(WebSocketEventType type, std::string payload) {
  if (this->websocket_event_mutex_ == nullptr) {
    return;
  }
  xSemaphoreTake(this->websocket_event_mutex_, portMAX_DELAY);

  // Livedata frames can arrive faster than ESPHome's main loop during a long
  // blocking operation. Only the newest consecutive frame is useful.
  if (type == WebSocketEventType::DATA && !this->pending_websocket_events_.empty() &&
      this->pending_websocket_events_.back().type == WebSocketEventType::DATA) {
    this->pending_websocket_events_.back().payload = std::move(payload);
    xSemaphoreGive(this->websocket_event_mutex_);
    return;
  }

  if (this->pending_websocket_events_.size() >= MAX_PENDING_WEBSOCKET_EVENTS) {
    auto drop = this->pending_websocket_events_.begin();
    for (auto event = this->pending_websocket_events_.begin(); event != this->pending_websocket_events_.end();
         ++event) {
      if (event->type == WebSocketEventType::DATA) {
        drop = event;
        break;
      }
    }
    this->pending_websocket_events_.erase(drop);
  }

  this->pending_websocket_events_.push_back({type, std::move(payload)});
  xSemaphoreGive(this->websocket_event_mutex_);
}

void OpenDtuMeterBridge::process_pending_websocket_events_() {
  if (this->websocket_event_mutex_ == nullptr || this->data_mutex_ == nullptr) {
    return;
  }

  std::vector<PendingWebSocketEvent> events;
  xSemaphoreTake(this->websocket_event_mutex_, portMAX_DELAY);
  events.swap(this->pending_websocket_events_);
  xSemaphoreGive(this->websocket_event_mutex_);

  for (auto &event : events) {
    switch (event.type) {
      case WebSocketEventType::CONNECTED:
        xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
        this->ws_connected_ = true;
        xSemaphoreGive(this->data_mutex_);
        ESP_LOGI(TAG, "WebSocket connected to OpenDTU");
        this->publish_state_();
        break;
      case WebSocketEventType::DISCONNECTED:
        xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
        this->ws_connected_ = false;
        xSemaphoreGive(this->data_mutex_);
        ESP_LOGW(TAG, "WebSocket disconnected, using fallback Modbus values");
        this->mark_data_stale_and_reset_();
        break;
      case WebSocketEventType::ERROR:
        xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
        this->ws_connected_ = false;
        xSemaphoreGive(this->data_mutex_);
        ESP_LOGW(TAG, "WebSocket error, using fallback Modbus values");
        this->mark_data_stale_and_reset_();
        break;
      case WebSocketEventType::DATA:
        this->process_livedata_(event.payload.data(), event.payload.size());
        break;
    }
  }
}

void OpenDtuMeterBridge::websocket_event_handler_(void *handler_args, esp_event_base_t base, int32_t event_id,
                                                  void *event_data) {
  auto *self = static_cast<OpenDtuMeterBridge *>(handler_args);
  if (self == nullptr) {
    return;
  }

  auto *data = static_cast<esp_websocket_event_data_t *>(event_data);

  switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      self->queue_websocket_event_(WebSocketEventType::CONNECTED);
      break;
    case WEBSOCKET_EVENT_DISCONNECTED:
      self->queue_websocket_event_(WebSocketEventType::DISCONNECTED);
      break;
    case WEBSOCKET_EVENT_DATA:
      if (data == nullptr) {
        break;
      }
      if (data->op_code != 0x01 && data->op_code != 0x00) {
        break;
      }
      if (data->payload_len <= 0 || data->payload_offset < 0 || data->data_len < 0) {
        break;
      }
      {
        const size_t payload_len = (size_t) data->payload_len;
        const size_t payload_offset = (size_t) data->payload_offset;
        const size_t data_len = (size_t) data->data_len;
        if (payload_offset > payload_len || data_len > payload_len - payload_offset ||
            (data_len > 0 && data->data_ptr == nullptr)) {
          ESP_LOGW(TAG, "Ignoring invalid WebSocket fragment");
          break;
        }
        if (!self->ws_buf_ensure_(payload_len + 1)) {
          break;
        }
        if (data_len > 0) {
          memcpy(self->ws_buf_ + payload_offset, data->data_ptr, data_len);
        }
        if (payload_offset + data_len == payload_len) {
          self->ws_buf_[payload_len] = '\0';
          self->queue_websocket_event_(WebSocketEventType::DATA, std::string(self->ws_buf_, payload_len));
        }
      }
      break;
    case WEBSOCKET_EVENT_ERROR:
      self->queue_websocket_event_(WebSocketEventType::ERROR);
      break;
    default:
      break;
  }
}

void OpenDtuMeterBridge::start_websocket_() {
  if (this->ws_started_) {
    return;
  }

  this->ws_uri_ = "ws://" + this->host_ + ":" + std::to_string(this->port_) + this->path_;

  const std::string userpass = this->username_ + ":" + this->password_;
  size_t b64_capacity = 0;
  int b64_result = mbedtls_base64_encode(nullptr, 0, &b64_capacity,
                                         reinterpret_cast<const unsigned char *>(userpass.data()), userpass.size());
  if (b64_result != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL || b64_capacity == 0 || b64_capacity == SIZE_MAX) {
    ESP_LOGE(TAG, "Failed to determine Basic Auth header size (error=%d)", b64_result);
    return;
  }

  std::vector<unsigned char> b64(b64_capacity);
  size_t b64_len = 0;
  b64_result = mbedtls_base64_encode(b64.data(), b64.size(), &b64_len,
                                     reinterpret_cast<const unsigned char *>(userpass.data()), userpass.size());
  if (b64_result != 0) {
    ESP_LOGE(TAG, "Failed to encode Basic Auth header (error=%d)", b64_result);
    return;
  }

  this->ws_auth_header_ = "Authorization: Basic ";
  this->ws_auth_header_.append(reinterpret_cast<const char *>(b64.data()), b64_len);
  this->ws_auth_header_.append("\r\n");

  esp_websocket_client_config_t cfg = {};
  cfg.uri = this->ws_uri_.c_str();
  cfg.headers = this->ws_auth_header_.c_str();
  cfg.reconnect_timeout_ms = 5000;
  cfg.network_timeout_ms = 10000;
  cfg.disable_auto_reconnect = false;

  this->ws_client_ = esp_websocket_client_init(&cfg);
  if (this->ws_client_ == nullptr) {
    ESP_LOGE(TAG, "Failed to initialize WebSocket client");
    return;
  }

  esp_websocket_register_events(this->ws_client_, WEBSOCKET_EVENT_ANY,
                                &OpenDtuMeterBridge::websocket_event_handler_, this);
  esp_err_t err = esp_websocket_client_start(this->ws_client_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WebSocket client: %s", esp_err_to_name(err));
    esp_websocket_client_destroy(this->ws_client_);
    this->ws_client_ = nullptr;
    return;
  }

  this->ws_started_ = true;
  ESP_LOGI(TAG, "Starting WebSocket client: %s (user=%s)", this->ws_uri_.c_str(), this->username_.c_str());
}

void OpenDtuMeterBridge::stop_websocket_() {
  if (this->ws_client_ != nullptr) {
    esp_websocket_client_stop(this->ws_client_);
    esp_websocket_client_destroy(this->ws_client_);
    this->ws_client_ = nullptr;
  }
  this->ws_started_ = false;
  if (this->websocket_event_mutex_ != nullptr) {
    xSemaphoreTake(this->websocket_event_mutex_, portMAX_DELAY);
    this->pending_websocket_events_.clear();
    xSemaphoreGive(this->websocket_event_mutex_);
  }
  if (this->data_mutex_ != nullptr) {
    xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
    this->ws_connected_ = false;
    xSemaphoreGive(this->data_mutex_);
  }
}

float OpenDtuMeterBridge::get_voltage(int phase) {
  if (phase < 1 || phase > 3) {
    return this->default_voltage_;
  }
  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  float value = this->data_stale_
                    ? this->default_voltage_
                    : this->modbus_voltage(this->phase_[phase].voltage, this->phase_[phase].has_data);
  xSemaphoreGive(this->data_mutex_);
  return value;
}

float OpenDtuMeterBridge::get_current(int phase) {
  if (phase < 1 || phase > 3) {
    return 0.0f;
  }
  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  float value = this->data_stale_
                    ? 0.0f
                    : this->modbus_current_power(this->phase_[phase].current, this->phase_[phase].has_data);
  xSemaphoreGive(this->data_mutex_);
  return value;
}

float OpenDtuMeterBridge::get_power(int phase) {
  if (phase < 1 || phase > 3) {
    return 0.0f;
  }
  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  float value = this->data_stale_
                    ? 0.0f
                    : this->modbus_current_power(this->phase_[phase].power, this->phase_[phase].has_data);
  xSemaphoreGive(this->data_mutex_);
  return value;
}

float OpenDtuMeterBridge::get_total_power() {
  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  float total = 0.0f;
  if (!this->data_stale_) {
    for (int phase = 1; phase <= 3; phase++) {
      total += this->modbus_current_power(this->phase_[phase].power, this->phase_[phase].has_data);
    }
  }
  xSemaphoreGive(this->data_mutex_);
  if (!float_is_finite(total)) {
    return 0.0f;
  }
  return total;
}

float OpenDtuMeterBridge::get_frequency() {
  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  float value = this->data_stale_
                    ? this->default_frequency_
                    : this->modbus_frequency(this->measured_frequency_, this->has_frequency_data_);
  xSemaphoreGive(this->data_mutex_);
  return value;
}

bool OpenDtuMeterBridge::is_data_valid() {
  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  bool valid = !this->data_stale_ && this->ws_connected_;
  xSemaphoreGive(this->data_mutex_);
  return valid;
}

bool OpenDtuMeterBridge::is_websocket_connected() {
  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  bool connected = this->ws_connected_;
  xSemaphoreGive(this->data_mutex_);
  return connected;
}

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 6, 0)
modbus::ResponseStatus MeterBridgeModbusServer::on_read_registers(uint16_t start_address,
                                                                  uint16_t number_of_registers,
                                                                  modbus::RegisterValues &registers) {
  if (this->bridge_ == nullptr) {
    return modbus::ModbusExceptionCode::SERVICE_DEVICE_FAILURE;
  }
  if (number_of_registers == 0 || number_of_registers > MAX_MODBUS_READ_REGISTERS) {
    return modbus::ModbusExceptionCode::ILLEGAL_DATA_VALUE;
  }

  std::vector<uint8_t> response;
  if (!this->bridge_->read_modbus_registers(start_address, number_of_registers, response)) {
    return modbus::ModbusExceptionCode::ILLEGAL_DATA_ADDRESS;
  }

  for (size_t offset = 0; offset < response.size(); offset += 2) {
    registers.push_back((uint16_t) (((uint16_t) response[offset] << 8) | response[offset + 1]));
  }
  return std::nullopt;
}
#else
void MeterBridgeModbusServer::on_modbus_read_registers(uint8_t function_code, uint16_t start_address,
                                                       uint16_t number_of_registers) {
  if (function_code != 0x03 && function_code != 0x04) {
    return;
  }
  if (this->bridge_ == nullptr) {
    return;
  }
  if (number_of_registers == 0 || number_of_registers > MAX_MODBUS_READ_REGISTERS) {
    std::vector<uint8_t> error_response;
    error_response.push_back(this->address_);
    error_response.push_back(function_code | 0x80);
    error_response.push_back(0x03);
    this->send_raw(error_response);
    return;
  }

  std::vector<uint8_t> response;
  if (!this->bridge_->read_modbus_registers(start_address, number_of_registers, response)) {
    std::vector<uint8_t> error_response;
    error_response.push_back(this->address_);
    error_response.push_back(function_code | 0x80);
    error_response.push_back(0x02);
    this->send_raw(error_response);
    return;
  }

  this->send(function_code, start_address, number_of_registers, response.size(), response.data());
}
#endif

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 6, 0)
void OpenDtuMeterBridge::set_modbus_server(modbus::ModbusServerHub *parent, uint8_t slave_address) {
#else
void OpenDtuMeterBridge::set_modbus_server(modbus::Modbus *parent, uint8_t slave_address) {
#endif
  this->modbus_parent_ = parent;
  this->modbus_slave_address_ = slave_address;
}

void OpenDtuMeterBridge::setup() {
  this->data_mutex_ = xSemaphoreCreateMutex();
  this->websocket_event_mutex_ = xSemaphoreCreateMutex();
  if (this->data_mutex_ == nullptr || this->websocket_event_mutex_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create component mutexes");
    this->mark_failed();
    return;
  }
  this->sync_modbus_registers_();
  if (this->modbus_parent_ != nullptr && this->modbus_slave_address_ != 0) {
    this->modbus_server_device_.set_bridge(this);
#if ESPHOME_VERSION_CODE < VERSION_CODE(2026, 6, 0)
    this->modbus_server_device_.set_parent(this->modbus_parent_);
#endif
    this->modbus_server_device_.set_address(this->modbus_slave_address_);
    this->modbus_parent_->register_device(&this->modbus_server_device_);

#if ESPHOME_VERSION_CODE < VERSION_CODE(2026, 6, 0)
    if (this->modbus_slave_address_ != DEYE_MAIN_METER_ADDRESS) {
      this->modbus_silence_device_.set_parent(this->modbus_parent_);
      this->modbus_silence_device_.set_address(DEYE_MAIN_METER_ADDRESS);
      this->modbus_parent_->register_device(&this->modbus_silence_device_);
    }
#endif
  }
#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
  wifi::global_wifi_component->add_connect_state_listener(this);
#endif
  this->publish_state_();
#ifdef USE_TEXT_SENSOR
  if (this->component_version_text_sensor_ != nullptr) {
    this->component_version_text_sensor_->publish_state(this->component_version_);
  }
#endif
}

#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
void OpenDtuMeterBridge::on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6>) {
  if (ssid.empty()) {
    ESP_LOGW(TAG, "WiFi disconnected, stopping WebSocket client");
    this->stop_websocket_();
    this->mark_data_stale_and_reset_();
    return;
  }
  if (!this->ws_started_) {
    this->start_websocket_();
  }
}
#endif

void OpenDtuMeterBridge::loop() {
  this->process_pending_websocket_events_();

#ifdef USE_WIFI
  if (!this->ws_started_ && wifi::global_wifi_component->is_connected()) {
    this->start_websocket_();
  }
#endif

  xSemaphoreTake(this->data_mutex_, portMAX_DELAY);
  const bool data_stale = this->data_stale_;
  const int64_t last_data_us = this->last_data_us_;
  xSemaphoreGive(this->data_mutex_);

  if (!data_stale) {
    int64_t age = esp_timer_get_time() - last_data_us;
    if (age > (int64_t) this->data_timeout_ms_ * 1000) {
      ESP_LOGW(TAG, "No livedata from OpenDTU for %lld ms, using fallback Modbus values", (long long) (age / 1000));
      this->mark_data_stale_and_reset_();
    }
  }
}

void OpenDtuMeterBridge::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenDTU meter bridge:");
  ESP_LOGCONFIG(TAG, "  Component version: %s", this->component_version_.c_str());
  ESP_LOGCONFIG(TAG, "  OpenDTU endpoint : %s:%u%s", this->host_.c_str(), this->port_, this->path_.c_str());
  ESP_LOGCONFIG(TAG, "  Username       : %s", this->username_.c_str());
  ESP_LOGCONFIG(TAG, "  Meter profile  : %s", meter_profile_name(this->meter_profile_));
  ESP_LOGCONFIG(TAG, "  Modbus address : 0x%02X", this->modbus_slave_address_);
  ESP_LOGCONFIG(TAG, "  Data timeout   : %u ms", this->data_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Fallback V/f   : %.1f V / %.1f Hz", this->default_voltage_, this->default_frequency_);
  ESP_LOGCONFIG(TAG, "  Microinverters : %u mapped", (unsigned) this->microinverter_map_.size());
  for (size_t i = 0; i < this->microinverter_map_.size(); i++) {
    const auto &entry = this->microinverter_map_[i];
    if (!entry.inverter_serial.empty()) {
      ESP_LOGCONFIG(TAG, "    [%u] serial='%s' -> grid phase L%u", (unsigned) (i + 1), entry.inverter_serial.c_str(),
                    entry.grid_phase);
    } else {
      ESP_LOGCONFIG(TAG, "    [%u] name='%s' -> grid phase L%u", (unsigned) (i + 1), entry.inverter_name.c_str(),
                    entry.grid_phase);
    }
  }
}

#ifdef USE_BUTTON
void RebootDeviceButton::dump_config() { LOG_BUTTON("", "Reboot Device Button", this); }

void RebootDeviceButton::press_action() { App.safe_reboot(); }
#endif

}  // namespace esphome::opendtu_meter_bridge
