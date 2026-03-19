#include "fsr_reader.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

static const char *TAG = "FSR_READER";

// ADC handles
static esp_adc_cal_characteristics_t adc_chars;
static bool is_initialized = false;

// Calibration values (will be learned per user)
static uint16_t cal_min = 0;
static uint16_t cal_max = 4095;

void fsr_reader_init(void)
{
    ESP_LOGI(TAG, "Initializing FSR reader...");
    
    // Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(FSR_ADC_CHANNEL, ADC_ATTEN_DB_12);
    
    // Calibrate ADC
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, 
                            ADC_WIDTH_BIT_12, 0, &adc_chars);
    
    is_initialized = true;
    ESP_LOGI(TAG, "FSR reader initialized");
}

fsr_data_t fsr_read_channel(uint8_t channel)
{
    if (!is_initialized) {
        fsr_reader_init();
    }
    
    fsr_data_t data = {0};
    
    // Read raw ADC (channel param ignored for now, but ready for MUX)
    data.raw_value = adc1_get_raw(FSR_ADC_CHANNEL);
    
    // Convert to voltage
    data.voltage_mv = esp_adc_cal_raw_to_voltage(data.raw_value, &adc_chars);
    
    // Calculate pressure percentage (0-100%)
    if (data.raw_value <= cal_min) {
        data.pressure_percent = 0;
    } else if (data.raw_value >= cal_max) {
        data.pressure_percent = 100;
    } else {
        data.pressure_percent = (data.raw_value - cal_min) * 100 / 
                                (cal_max - cal_min);
    }
    
    // Set pressure level
    if (data.pressure_percent < 5) {
        data.level = PRESSURE_NONE;
    } else if (data.pressure_percent < 30) {
        data.level = PRESSURE_LIGHT;
    } else if (data.pressure_percent < 70) {
        data.level = PRESSURE_MEDIUM;
    } else {
        data.level = PRESSURE_HARD;
    }
    
    return data;
}

void fsr_read_all(fsr_data_t* data_array, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        data_array[i] = fsr_read_channel(i);
    }
}

void fsr_calibrate_min(void)
{
    ESP_LOGI(TAG, "Calibrating MIN pressure (keep finger OFF sensor)...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    uint32_t sum = 0;
    for (int i = 0; i < 50; i++) {
        sum += adc1_get_raw(FSR_ADC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    cal_min = sum / 50;
    ESP_LOGI(TAG, "MIN calibration complete: %d", cal_min);
}

void fsr_calibrate_max(void)
{
    ESP_LOGI(TAG, "Calibrating MAX pressure (press HARD on sensor NOW)...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    uint32_t max = 0;
    for (int i = 0; i < 50; i++) {
        int val = adc1_get_raw(FSR_ADC_CHANNEL);
        if (val > max) max = val;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    cal_max = max;
    ESP_LOGI(TAG, "MAX calibration complete: %d", cal_max);
}

void fsr_set_calibration(uint16_t min, uint16_t max)
{
    cal_min = min;
    cal_max = max;
    ESP_LOGI(TAG, "Calibration set: min=%d, max=%d", min, max);
}