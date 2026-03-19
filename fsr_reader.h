#ifndef FSR_READER_H
#define FSR_READER_H

#include <stdint.h>

// FSR channel configuration
#define FSR_CHANNEL_COUNT 1  // Will expand to 8-10 with multiplexer
#define FSR_ADC_CHANNEL ADC_CHANNEL_4  // GPIO4

// Pressure levels
typedef enum {
    PRESSURE_NONE = 0,
    PRESSURE_LIGHT,
    PRESSURE_MEDIUM,
    PRESSURE_HARD
} pressure_level_t;

// FSR data structure
typedef struct {
    uint16_t raw_value;
    uint32_t voltage_mv;
    uint8_t  pressure_percent;
    pressure_level_t level;
} fsr_data_t;

// Initialize FSR reader
void fsr_reader_init(void);

// Read single FSR channel
fsr_data_t fsr_read_channel(uint8_t channel);

// Read all channels (for future multiplexer use)
void fsr_read_all(fsr_data_t* data_array, uint8_t count);

// Calibration functions
void fsr_calibrate_min(void);
void fsr_calibrate_max(void);
void fsr_set_calibration(uint16_t min, uint16_t max);

#endif // FSR_READER_H