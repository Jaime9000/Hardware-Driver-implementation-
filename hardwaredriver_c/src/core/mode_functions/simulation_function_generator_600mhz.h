#ifndef SIMULATION_FUNCTION_GENERATOR_600MHZ_H
#define SIMULATION_FUNCTION_GENERATOR_600MHZ_H

// System headers
#include <stdint.h>
#include <stddef.h>

// Constants
#define SIMULATION_SAMPLE_WIDTH 16
#define SIMULATION_SAMPLE_COUNT 70

// Function declarations
const uint8_t* get_simulation_sample_data(size_t sample_index);
size_t get_simulation_sample_count(void);
size_t get_simulation_sample_width(void);

#endif // SIMULATION_FUNCTION_GENERATOR_600MHZ_H