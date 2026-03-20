#ifndef FSR_DATA_TYPES_H
#define FSR_DATA_TYPES_H

#include <stdint.h>

// Single definition of fsr_data_t for all modules
typedef struct {
    int raw_value;           // Raw ADC value (0-4095)
    int voltage_mv;          // Calculated voltage in mV
    const char* pressure_level;  // String description
} fsr_data_t;

#endif // FSR_DATA_TYPES_H