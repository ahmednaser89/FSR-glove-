#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "FSR_DEMO";

// ADC settings
#define ADC_CHANNEL ADC_CHANNEL_4        // GPIO4 = ADC1_CH4 on ESP32-S3
#define ADC_UNIT ADC_UNIT_1

// Handles
adc_oneshot_unit_handle_t adc_handle;
adc_cali_handle_t cali_handle;

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing ADC...");
    
    // ---------- Initialize ADC Oneshot ----------
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));
    
    // ---------- Initialize ADC Calibration ----------
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_12,
    };
    
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle));
    
    ESP_LOGI(TAG, "FSR Sensor Demo Started!");
    ESP_LOGI(TAG, "Reading from GPIO4 (ADC1_CH4)");
    
    int raw_value;
    int voltage;
    
    while (1) {
        // Read raw ADC value
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_value));
        
        // Convert to voltage
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw_value, &voltage));
        
        // Print readings
        printf("\n");
        printf("Raw ADC: %4d | Voltage: %3d mV | ", raw_value, voltage);
        
        // Simple pressure classification
        if (raw_value < 100) {
            printf("Pressure: NO TOUCH");
        } else if (raw_value < 1000) {
            printf("Pressure: LIGHT TOUCH");
        } else if (raw_value < 2500) {
            printf("Pressure: MEDIUM PRESS");
        } else {
            printf("Pressure: HARD PRESS!");
        }
        
        // Wait before next reading
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}