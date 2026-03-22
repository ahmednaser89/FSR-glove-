#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "ble_module.h"
#include "fsr_reader.h"
#include "fsr_data_types.h"

static const char *TAG = "FSR_DEMO";

// Add a status task to monitor BLE connection
void ble_status_task(void *pvParameters)
{
    while (1) {
        ble_print_status();
        vTaskDelay(pdMS_TO_TICKS(500));  // Print status every 500 ms
    }
}

void fsr_monitor_task(void *pvParameters)
{
    fsr_internal_data_t fsr_data;
    char send_buffer[16];
    uint32_t last_send_time = 0;
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    while (1) {
        // Always read ADC at full speed (100Hz)
        fsr_data = fsr_read_channel(0);
        
        // Build visual bar
        char bar[21];
        int bar_len = fsr_data.pressure_percent / 5;
        for (int i = 0; i < 20; i++) {
            bar[i] = (i < bar_len) ? '#' : '.';
        }
        bar[20] = '\0';
        
        // Update console display every time (fast)
        printf("\r\033[KADC: %4d | %4ld mV | %-12s | %3d%% [%s]",
               fsr_data.raw_value,
               (long)fsr_data.voltage_mv,
               fsr_data.level_str,
               fsr_data.pressure_percent,
               bar);
        fflush(stdout);
        
        // Send BLE data only at reduced rate (e.g., every 500ms)
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_send_time >= 500) {  // Send every 500ms (2Hz)
            snprintf(send_buffer, sizeof(send_buffer), "%d", fsr_data.raw_value);
            ble_send_pressure_string(send_buffer);
            last_send_time = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // 100Hz ADC reading
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "FSR Glove Starting with 10kΩ resistor on GPIO14");
    ESP_LOGI(TAG, "Wiring: 3.3V → FSR → GPIO14 → 10kΩ → GND");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize FSR reader
    fsr_reader_init();

    // calibration

if (!fsr_load_calibration()) {


     ESP_LOGI(TAG, "Starting calibration in 2 seconds...");
     vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Starting MIN calibration. DO NOT TOUCH SENSOR.");
    fsr_calibrate_min();
    ESP_LOGI(TAG, "Starting MAX calibration. SQUEEZE HARD IN 5 SECONDS!");
    fsr_calibrate_max();
    fsr_save_calibration();
}



    // Initialize BLE
    ESP_LOGI(TAG, "Initializing BLE...");
    ble_module_init();
    vTaskDelay(pdMS_TO_TICKS(1000));
    ble_module_start();
// In app_main(), add this after starting BLE:
//xTaskCreate(ble_status_task, "ble_status", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "System ready!");
    ESP_LOGI(TAG, "Look for 'ESP32_FSR_Glove' in BLE scanner");

    // Create monitoring task
    xTaskCreate(fsr_monitor_task, "fsr_monitor", 8192, NULL, 5, NULL);
}



