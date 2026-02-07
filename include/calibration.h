#pragma once

#include <stdint.h>

typedef struct
{
	uint16_t adc_min;
	uint16_t adc_max;
} calib_data_t;

// Returns 1 if valid data was loaded, 0 otherwise.
uint8_t calib_load(calib_data_t *out);

// Returns 1 on success, 0 on failure.
uint8_t calib_save(const calib_data_t *in);
