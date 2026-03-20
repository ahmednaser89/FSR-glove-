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

// Custom pressure levels based on raw ADC
static const char* get_pressure_level_custom(int raw) {
    if (raw < 800) return "NO TOUCH";
    if (raw < 1800) return "LIGHT TOUCH";
    if (raw < 3200) return "MEDIUM PRESS";
    return "HARD PRESS!";
}

void fsr_monitor_task(void *pvParameters)
{
    fsr_internal_data_t fsr_data;
    fsr_data_t ble_data;
    char send_buffer[128];

    vTaskDelay(pdMS_TO_TICKS(2000));

    printf("\033[2J\033[H");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║      FSR GLOVE PRESSURE MONITOR (GPIO14 / ADC2_CH3)     ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    printf("Press the sensor to see real-time readings...\n\n");

    while (1) {
        fsr_data = fsr_read_channel(0);

        // Create bar graph
        char bar[21];
        int bar_len = fsr_data.pressure_percent / 5;
        for (int i = 0; i < 20; i++) {
            bar[i] = (i < bar_len) ? '#' : '.';
        }
        bar[20] = '\0';

        printf("\r\033[K");
        printf("ADC: %4d | %4ld mV | %s | %3d%% [%s]",
               fsr_data.raw_value,
               (long)fsr_data.voltage_mv,
               fsr_data.level_str,
               fsr_data.pressure_percent,
               bar);
        fflush(stdout);

        // Prepare BLE string
        const char* custom_level = get_pressure_level_custom(fsr_data.raw_value);
        snprintf(send_buffer, sizeof(send_buffer),
                 "FSR:%d,%ld,%s",
                 fsr_data.raw_value,
                 (long)fsr_data.voltage_mv,
                 custom_level);

        ble_send_pressure_string(send_buffer);
        fsr_to_ble_data(&fsr_data, &ble_data);
        ble_send_fsr_data(&ble_data, 1);

        vTaskDelay(pdMS_TO_TICKS(200));
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

    // Optional: calibration
    // ESP_LOGI(TAG, "Starting calibration in 2 seconds...");
    // vTaskDelay(pdMS_TO_TICKS(2000));
    // fsr_calibrate_min();
    // fsr_calibrate_max();

    // Initialize BLE
    ESP_LOGI(TAG, "Initializing BLE...");
    ble_module_init();
    vTaskDelay(pdMS_TO_TICKS(1000));
    ble_module_start();

    ESP_LOGI(TAG, "System ready!");
    ESP_LOGI(TAG, "Look for 'ESP32_FSR_Glove' in BLE scanner");

    // Create monitoring task
    xTaskCreate(fsr_monitor_task, "fsr_monitor", 4096, NULL, 5, NULL);
}