#ifndef BLE_MODULE_H
#define BLE_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include "fsr_data_types.h"

// Initialize BLE
void ble_module_init(void);

// Start advertising
void ble_module_start(void);

// Send FSR data over BLE
void ble_send_fsr_data(fsr_data_t* data, uint8_t count);

// Send formatted pressure string
void ble_send_pressure_string(const char* str);

// Debug function
void ble_print_status(void);

#endif // BLE_MODULE_H