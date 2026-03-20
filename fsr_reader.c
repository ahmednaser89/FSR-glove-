#include "fsr_reader.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "FSR_READER";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;
static bool is_initialized = false;

// Calibration values
static uint16_t cal_min = 0;
static uint16_t cal_max = 4095;
static bool calibrated = false;

void fsr_reader_init(void)
{
    if (is_initialized) return;

    ESP_LOGI(TAG, "Initializing FSR reader on GPIO14 (ADC2_CH3) with 10kΩ resistor...");

    // ADC Oneshot initialization
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = FSR_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC: %s", esp_err_to_name(ret));
        return;
    }
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    
    ret = adc_oneshot_config_channel(adc_handle, FSR_ADC_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        return;
    }
    
    // Initialize ADC calibration
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = FSR_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Calibration not available: %s", esp_err_to_name(ret));
        cali_handle = NULL;
    } else {
        ESP_LOGI(TAG, "ADC calibration initialized");
    }
    
    is_initialized = true;
    ESP_LOGI(TAG, "ADC2 initialized on GPIO14");
}

const char* fsr_get_level_string(pressure_level_t level)
{
    switch(level) {
        case PRESSURE_NONE:   return "NO TOUCH";
        case PRESSURE_LIGHT:  return "LIGHT TOUCH";
        case PRESSURE_MEDIUM: return "MEDIUM PRESS";
        case PRESSURE_HARD:   return "HARD PRESS!";
        default:              return "UNKNOWN";
    }
}

fsr_internal_data_t fsr_read_channel(uint8_t channel)
{
    if (!is_initialized) {
        fsr_reader_init();
    }
    
    fsr_internal_data_t data = {0};
    
    // Read ADC value
    int raw_value;
    esp_err_t ret = adc_oneshot_read(adc_handle, FSR_ADC_CHANNEL, &raw_value);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(ret));
        data.raw_value = 0;
        data.voltage_mv = 0;
    } else {
        data.raw_value = raw_value;
        
        // Convert to voltage if calibration is available
        if (cali_handle != NULL) {
            int voltage;
            ret = adc_cali_raw_to_voltage(cali_handle, raw_value, &voltage);
            if (ret == ESP_OK) {
                data.voltage_mv = voltage;
            } else {
                // Fallback voltage calculation
                data.voltage_mv = raw_value * 3300 / 4095;
            }
        } else {
            // Fallback voltage calculation
            data.voltage_mv = raw_value * 3300 / 4095;
        }
    }
    
    // Print raw value immediately (debug)
    printf("\r\033[KRaw ADC: %4d  ", data.raw_value);
    fflush(stdout);
    
    // Calculate pressure percentage
    if (calibrated && cal_max > cal_min) {
        if (data.raw_value <= cal_min) {
            data.pressure_percent = 0;
        } else if (data.raw_value >= cal_max) {
            data.pressure_percent = 100;
        } else {
            data.pressure_percent = (data.raw_value - cal_min) * 100 / (cal_max - cal_min);
        }
    } else {
        // Fallback: use raw value directly
        data.pressure_percent = data.raw_value * 100 / 4095;
    }
    
    // Assign pressure level
    if (data.pressure_percent < 10)      data.level = PRESSURE_NONE;
    else if (data.pressure_percent < 30) data.level = PRESSURE_LIGHT;
    else if (data.pressure_percent < 70) data.level = PRESSURE_MEDIUM;
    else                                 data.level = PRESSURE_HARD;
    
    snprintf(data.level_str, sizeof(data.level_str), "%s",
             fsr_get_level_string(data.level));
    
    return data;
}

void fsr_to_ble_data(fsr_internal_data_t* internal, fsr_data_t* ble_data)
{
    if (internal && ble_data) {
        ble_data->raw_value = internal->raw_value;
        ble_data->voltage_mv = internal->voltage_mv;
        ble_data->pressure_level = internal->level_str;
    }
}

void fsr_read_all(fsr_internal_data_t* data_array, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        data_array[i] = fsr_read_channel(i);
    }
}

void fsr_calibrate_min(void)
{
    if (!is_initialized) fsr_reader_init();
    
    ESP_LOGI(TAG, "Calibrating MIN – keep sensor untouched...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    uint32_t sum = 0;
    int val;
    for (int i = 0; i < 50; i++) {
        adc_oneshot_read(adc_handle, FSR_ADC_CHANNEL, &val);
        sum += val;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    cal_min = sum / 50;
    calibrated = true;
    ESP_LOGI(TAG, "MIN calibration complete: %d", cal_min);
}

void fsr_calibrate_max(void)
{
    if (!is_initialized) fsr_reader_init();
    
    ESP_LOGI(TAG, "Calibrating MAX – press HARD on sensor...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    uint32_t max = 0;
    int val;
    for (int i = 0; i < 50; i++) {
        adc_oneshot_read(adc_handle, FSR_ADC_CHANNEL, &val);
        if (val > max) max = val;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    cal_max = max;
    calibrated = true;
    ESP_LOGI(TAG, "MAX calibration complete: %d", cal_max);
}

void fsr_set_calibration(uint16_t min, uint16_t max)
{
    cal_min = min;
    cal_max = max;
    calibrated = true;
    ESP_LOGI(TAG, "Calibration set: min=%d, max=%d", min, max);
}

bool fsr_is_calibrated(void)
{
    return calibrated;
}