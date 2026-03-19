#include "ble_module.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"

static const char *TAG = "BLE_MODULE";

// BLE attributes
static uint8_t adv_data[32];
static uint16_t adv_config_done = 0;
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// GATT interface
static uint16_t gatts_if = ESP_GATT_IF_NONE;
static uint16_t app_id = 0;
static uint16_t conn_id = 0;
static bool is_connected = false;

// Service and characteristic handles
static uint16_t service_handle;
static uint16_t char_handle;
static uint16_t descr_handle;

// Forward declarations
static void gatts_event_handler(esp_gatts_cb_event_t event, 
                                esp_gatt_if_t gatts_if, 
                                esp_ble_gatts_cb_param_t *param);

void ble_module_init(void)
{
    ESP_LOGI(TAG, "Initializing BLE...");
    
    // Initialize Bluetooth controller
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    
    // Initialize Bluedroid stack
    esp_bluedroid_init();
    esp_bluedroid_enable();
    
    // Register GATT callback
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(app_id);
    
    ESP_LOGI(TAG, "BLE initialized");
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event,
                                       esp_gatt_if_t gatts_if,
                                       esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(TAG, "GATT registered, status %d", param->reg.status);
            gatts_if = gatts_if;
            
            // Create service table
            esp_gatts_create_attr_tab(NULL, gatts_if, 0, 0);
            break;
            
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            ESP_LOGI(TAG, "Attribute table created");
            
            // Start service
            esp_ble_gatts_start_service(service_handle);
            
            // Set advertisement data
            esp_ble_gap_config_adv_data_raw(adv_data, sizeof(adv_data));
            break;
            
        case ESP_GATTS_START_EVT:
            ESP_LOGI(TAG, "Service started");
            
            // Start advertising
            esp_ble_gap_start_advertising(&adv_params);
            break;
            
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "Connected");
            is_connected = true;
            conn_id = param->connect.conn_id;
            break;
            
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Disconnected");
            is_connected = false;
            
            // Restart advertising
            esp_ble_gap_start_advertising(&adv_params);
            break;
            
        case ESP_GATTS_WRITE_EVT:
            // Handle incoming writes (e.g., calibration commands)
            ESP_LOGI(TAG, "Write event received");
            break;
            
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event,
                               esp_gatt_if_t gatts_if,
                               esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.app_id == app_id) {
            gatts_profile_event_handler(event, gatts_if, param);
        }
    }
}

void ble_module_start(void)
{
    ESP_LOGI(TAG, "Starting BLE advertising...");
    esp_ble_gap_start_advertising(&adv_params);
    ESP_LOGI(TAG, "Device name: ESP32_FSR_Glove");
}

void ble_send_fsr_data(fsr_data_t* data, uint8_t count)
{
    if (!is_connected) return;
    
    // Format data for BLE transmission
    uint8_t buffer[32];
    uint8_t len = 0;
    
    // Simple format: [count][raw1][raw2]...
    buffer[len++] = count;
    for (int i = 0; i < count; i++) {
        buffer[len++] = data[i].raw_value >> 8;
        buffer[len++] = data[i].raw_value & 0xFF;
        buffer[len++] = data[i].pressure_percent;
    }
    
    // Send notification
    esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle,
                                len, buffer, false);
}

void ble_send_pressure_string(const char* str)
{
    if (!is_connected) return;
    
    esp_ble_gatts_send_indicate(gatts_if, conn_id, char_handle,
                                strlen(str), (uint8_t*)str, false);
}