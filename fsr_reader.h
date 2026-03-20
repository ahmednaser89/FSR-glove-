#ifndef FSR_READER_H
#define FSR_READER_H

#include <stdint.h>
#include <stdbool.h>
#include "fsr_data_types.h"

// Pin configuration for GPIO14 (ADC2_CH3)
#define FSR_ADC_CHANNEL ADC_CHANNEL_3   // GPIO14 (ADC2_CH3)
#define FSR_ADC_UNIT   ADC_UNIT_2

// Internal pressure levels
typedef enum {
    PRESSURE_NONE = 0,
    PRESSURE_LIGHT,
    PRESSURE_MEDIUM,
    PRESSURE_HARD
} pressure_level_t;

// Internal data structure
typedef struct {
    uint16_t raw_value;
    uint32_t voltage_mv;
    uint8_t  pressure_percent;
    pressure_level_t level;
    char    level_str[16];
} fsr_internal_data_t;

// Initialization
void fsr_reader_init(void);

// Read a single channel
fsr_internal_data_t fsr_read_channel(uint8_t channel);

// Convert internal data to BLE format
void fsr_to_ble_data(fsr_internal_data_t* internal, fsr_data_t* ble_data);

// Read all channels
void fsr_read_all(fsr_internal_data_t* data_array, uint8_t count);

// Calibration functions
void fsr_calibrate_min(void);
void fsr_calibrate_max(void);
void fsr_set_calibration(uint16_t min, uint16_t max);
bool fsr_is_calibrated(void);

const char* fsr_get_level_string(pressure_level_t level);

#endif // FSR_READER_H