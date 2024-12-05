#ifndef SIMULATION_FUNCTION_GENERATOR_600MHZ_H
#define SIMULATION_FUNCTION_GENERATOR_600MHZ_H

#include <stdint.h>
#include <stddef.h>

#define SIMULATION_SAMPLE_WIDTH 16
#define SIMULATION_SAMPLE_COUNT 70

// Get simulated data for testing mode functions
const uint8_t* get_simulation_sample_data(size_t sample_index);
size_t get_simulation_sample_count(void);
size_t get_simulation_sample_width(void);

#endif // SIMULATION_FUNCTION_GENERATOR_600MHZ_H