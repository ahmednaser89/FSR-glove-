#ifndef BLE_MODULE_H
#define BLE_MODULE_H

#include <stdint.h>
#include "fsr_reader.h"

// BLE Service UUIDs
#define BLE_SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define BLE_CHARACTERISTIC_UUID  "12345678-1234-1234-1234-123456789013"

// Initialize BLE
void ble_module_init(void);

// Start advertising
void ble_module_start(void);

// Send FSR data over BLE
void ble_send_fsr_data(fsr_data_t* data, uint8_t count);

// Send formatted pressure string
void ble_send_pressure_string(const char* str);

#endif // BLE_MODULE_H