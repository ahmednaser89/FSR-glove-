#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gatt_common_api.h"

#include "ble_module.h"
#include "fsr_reader.h"

static const char *TAG = "BLE_GATT";

// UUIDs for your MIT App Inventor
static const uint8_t service_uuid[16] = {
    0x12, 0x90, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12, 
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12
};

static const uint8_t char_uuid[16] = {
    0x13, 0x90, 0x78, 0x56, 0x34, 0x12, 0x34, 0x12, 
    0x34, 0x12, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12
};

// CCCD UUID (standard for all BLE characteristics)
#define GATT_UUID_CHAR_CLIENT_CONFIG 0x2902

static const char device_name[] = "ESP32 FSR Glove";

// Advertising parameters
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// Advertising data with service UUID
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x20,
    .max_interval = 0x40,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 16,
    .p_service_uuid = (uint8_t*)service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// GATT variables
static uint16_t gatts_if_global = 0;
static uint16_t conn_id_global = 0;
static uint16_t service_handle = 0;
static uint16_t char_handle = 0;
static uint16_t descr_handle = 0;  // CCCD handle
static bool is_connected = false;
static bool notifications_enabled = false;

// Forward declarations
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void calibration_task(void *pvParameters);

void ble_module_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Release Classic BT memory
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    
    // Initialize controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init controller");
        return;
    }
    
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable controller");
        return;
    }
    
    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init bluedroid");
        return;
    }
    
    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable bluedroid");
        return;
    }
    
    // Register callbacks
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GAP callback");
        return;
    }
    
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GATTS callback");
        return;
    }
    
    // Register app
    ret = esp_ble_gatts_app_register(0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register app");
        return;
    }
    
    // Set device name
    ret = esp_ble_gap_set_device_name(device_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set device name");
        return;
    }
    
    ESP_LOGI(TAG, "BLE initialized");
}

void ble_module_start(void)
{
    ESP_LOGI(TAG, "Starting advertising...");
    
    // Configure advertising data
    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config adv data: %s", esp_err_to_name(ret));
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "Adv data set complete");
            esp_ble_gap_start_advertising(&adv_params);
            break;
            
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "✅ Advertising started");
            } else {
                ESP_LOGE(TAG, "Advertising start failed");
            }
            break;
            
        default:
            break;
    }
}

static void calibration_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting calibration task...");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Calibrating MIN - DO NOT TOUCH sensor");
    fsr_calibrate_min();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Calibrating MAX - PRESS HARD on sensor NOW");
    fsr_calibrate_max();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Saving calibration to NVS");
    fsr_save_calibration();
    
    ESP_LOGI(TAG, "✅ Calibration complete!");
    vTaskDelete(NULL);
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(TAG, "GATT registered, app_id: %d", param->reg.app_id);
            gatts_if_global = gatts_if;
            
            // Create the service with enough handles for service, char, and CCCD
            esp_gatt_srvc_id_t service_id = {
                .id = {
                    .uuid = {
                        .len = ESP_UUID_LEN_128,
                    },
                    .inst_id = 0,
                },
                .is_primary = true,
            };
            memcpy(service_id.id.uuid.uuid.uuid128, service_uuid, 16);
            
            esp_err_t ret2 = esp_ble_gatts_create_service(gatts_if, &service_id, 8);
            if (ret2 != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create service: %s", esp_err_to_name(ret2));
            }
            break;
            
        case ESP_GATTS_CREATE_EVT:
            ESP_LOGI(TAG, "Service created, service_handle: %d", param->create.service_handle);
            service_handle = param->create.service_handle;
            
            // Add characteristic with NOTIFY property
            esp_bt_uuid_t char_uuid_bt = {
                .len = ESP_UUID_LEN_128,
            };
            memcpy(char_uuid_bt.uuid.uuid128, char_uuid, 16);
            
            esp_gatt_char_prop_t property = ESP_GATT_CHAR_PROP_BIT_READ | 
                                            ESP_GATT_CHAR_PROP_BIT_WRITE | 
                                            ESP_GATT_CHAR_PROP_BIT_NOTIFY;
            esp_gatt_perm_t perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
            
            // Add the characteristic
            esp_err_t ret3 = esp_ble_gatts_add_char(
                service_handle, 
                &char_uuid_bt, 
                perm, 
                property, 
                NULL, 
                NULL);
            if (ret3 != ESP_OK) {
                ESP_LOGE(TAG, "Failed to add characteristic: %s", esp_err_to_name(ret3));
            }
            break;
            
case ESP_GATTS_ADD_CHAR_EVT:
            ESP_LOGI(TAG, "Characteristic added, handle: %d", param->add_char.attr_handle);
            char_handle = param->add_char.attr_handle;
            
            // Fix: We MUST add the CCCD (0x2902) descriptor so the client can enable notifications
            esp_bt_uuid_t descr_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}, // This is 0x2902
            };
            
            esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(
                service_handle, 
                &descr_uuid, 
                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 
                NULL, 
                NULL);
                
            if (add_descr_ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to add descriptor: %s", esp_err_to_name(add_descr_ret));
            }
            break;
            
        case ESP_GATTS_ADD_CHAR_DESCR_EVT:
            ESP_LOGI(TAG, "Descriptor added, handle: %d", param->add_char_descr.attr_handle);
            descr_handle = param->add_char_descr.attr_handle;
            
            // Fix: Start the service ONLY AFTER the descriptor has been successfully added
            esp_ble_gatts_start_service(service_handle);
            break;
            
        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "Service started - ready for connections!");
            break;
            
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "✅ Client connected, conn_id: %d", param->connect.conn_id);
            is_connected = true;
            conn_id_global = param->connect.conn_id;
            notifications_enabled = false;
            break;
            
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Client disconnected");
            is_connected = false;
            conn_id_global = 0;
            notifications_enabled = false;
            // Restart advertising
            esp_ble_gap_start_advertising(&adv_params);
            break;
            
// Replace your existing case ESP_GATTS_WRITE_EVT in ble_module.c
case ESP_GATTS_WRITE_EVT: {
    ESP_LOGI(TAG, "📝 GATT Write, handle: %d, len: %d", param->write.handle, param->write.len);
    
    // 1. Check if write is to CCCD (enable/disable notifications)
    if (param->write.handle == descr_handle) {
        if (param->write.len == 2) {
            uint16_t cccd_value = param->write.value[0] | (param->write.value[1] << 8);
            if (cccd_value == 0x0001) {
                notifications_enabled = true;
                ESP_LOGI(TAG, "✅🔔 Notifications ENABLED!");
                // Keep this short! "FSR:READY" is safer than long strings
                ble_send_pressure_string("READY"); 
            } else {
                notifications_enabled = false;
                ESP_LOGI(TAG, "🔕 Notifications DISABLED");
            }
        }
    }
    // 2. Check if write is to our characteristic (e.g., Calibration command)
    else if (param->write.handle == char_handle) {
        char received_data[32] = {0};
        int len = (param->write.len > 31) ? 31 : param->write.len;
        memcpy(received_data, param->write.value, len);
        
        if (strncmp(received_data, "CAL", 3) == 0) {
            ESP_LOGI(TAG, "📢 Calibration command received!");
            xTaskCreate(calibration_task, "calibration_task", 4096, NULL, 5, NULL);
        }
    }

    // MANDATORY FIX: Always respond to the phone if it expects an ACK
    if (param->write.need_rsp) {
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, 
                                    param->write.trans_id, ESP_GATT_OK, NULL);
    }
    break;
}
            
        default:
            break;
    }
}

void ble_send_pressure_string(const char* str)
{
    // Fix: Removed the conn_id_global != 0 check because 0 is a valid connection ID!
    if (is_connected && char_handle != 0 && notifications_enabled) {
        esp_err_t ret = esp_ble_gatts_send_indicate(
            gatts_if_global, 
            conn_id_global, 
            char_handle, 
            strlen(str), 
            (uint8_t*)str, 
            false  // false = notification, true = indication
        );
        
        if (ret == ESP_OK) {
            // Fix: Changed LOGD to LOGI so it actually prints in your standard terminal output
            ESP_LOGI(TAG, "📤 Sent: %s", str);
        } else {
            ESP_LOGW(TAG, "Failed to send: %s", esp_err_to_name(ret));
        }
    } else if (is_connected && !notifications_enabled) {
        static int log_counter = 0;
        if (log_counter++ % 100 == 0) {
            ESP_LOGI(TAG, "Waiting for client to enable notifications...");
            ESP_LOGI(TAG, "In nRF Connect, tap the DOWNLOAD icon next to the characteristic");
        }
    }
}

void ble_send_fsr_data(fsr_data_t* data, uint8_t count)
{

}

void ble_print_status(void)
{
    ESP_LOGI(TAG, "BLE - Connected: %s, Notifications: %s, conn_id: %d, char: %d", 
             is_connected ? "YES" : "NO", 
             notifications_enabled ? "ON" : "OFF",
             conn_id_global, char_handle);
}