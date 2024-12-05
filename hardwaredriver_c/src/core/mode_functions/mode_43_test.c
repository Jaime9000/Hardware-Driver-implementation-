#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mode_43.h"
#include "logger.h"
#include "simulation_function_generator_600mhz.h"

static int get_mode_number(const Mode* mode) {
    (void)mode;
    return 43;
}

static const uint8_t* get_emg_config(const Mode* mode, size_t* length) {
    Mode43Raw* raw_mode = (Mode43Raw*)mode->impl;
    *length = 1;
    
    static uint8_t config[1];
    switch(raw_mode->filter_type) {
        case NOTCH_FILTER_P: config[0] = 'p'; break;
        case NOTCH_FILTER_Q: config[0] = 'q'; break;
        case NOTCH_FILTER_R: config[0] = 'r'; break;
        case NOTCH_FILTER_S: config[0] = 's'; break;
        case NOTCH_FILTER_T: config[0] = 't'; break;
        case NOTCH_FILTER_U: config[0] = 'u'; break;
        case NOTCH_FILTER_V: config[0] = 'v'; break;
        case NOTCH_FILTER_W: config[0] = 'w'; break;
        default: config[0] = 'r'; break;
    }
    return config;
}

static ErrorCode execute(Mode* base, uint8_t* output, size_t* output_length) {
    Mode43Raw* mode = (Mode43Raw*)base->impl;
    static size_t current_sample = 0;
    
    // Handle first run initialization
    if (!mode->is_first_run) {
        // Simulate throwing away initial bytes using simulation data
        size_t bytes_thrown = 0;
        while (bytes_thrown < MODE_43_INIT_BYTES && current_sample < get_simulation_sample_count()) {
            const uint8_t* sample = get_simulation_sample_data(current_sample++);
            if (sample) {
                bytes_thrown += get_simulation_sample_width();
            }
        }
        mode->is_first_run = true;
    }

    // Read and process simulated data
    size_t total_collected = 0;
    size_t max_samples = MODE_43_MAX_COLLECT / get_simulation_sample_width();
    
    while (total_collected < max_samples && current_sample < get_simulation_sample_count()) {
        const uint8_t* sample = get_simulation_sample_data(current_sample++);
        if (!sample) break;
        
        // Copy sample to output buffer
        size_t sample_width = get_simulation_sample_width();
        if (total_collected * sample_width < *output_length) {
            memcpy(output + (total_collected * sample_width), sample, sample_width);
            total_collected++;
        }
    }

    // Reset sample counter if we've used all samples
    if (current_sample >= get_simulation_sample_count()) {
        current_sample = 0;
    }

    *output_length = total_collected * get_simulation_sample_width();
    return ERROR_NONE;
}

static ErrorCode execute_not_connected(Mode* base, uint8_t* output, size_t* output_length) {
    // Reference original pattern from mode_43.c
    static const uint8_t pattern[] = {
        0x80, 0x01, 0x90, 0x01, 0xA0, 0x01, 0xB0, 0x01,
        0xC0, 0x01, 0xD0, 0x01, 0xE0, 0x01, 0xF0, 0x01
    };

    size_t pattern_size = sizeof(pattern);
    size_t max_repeats = *output_length / pattern_size;
    
    for (size_t i = 0; i < max_repeats; i++) {
        memcpy(output + (i * pattern_size), pattern, pattern_size);
    }
    
    *output_length = max_repeats * pattern_size;
    return ERROR_NONE;
}

static const ModeVTable mode_43_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute = execute,
    .execute_not_connected = execute_not_connected,
    .destroy = NULL
};

ErrorCode mode_43_raw_create(Mode43Raw** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_43_raw_create");
        return ERROR_INVALID_PARAMETER;
    }

    Mode43Raw* new_mode = (Mode43Raw*)malloc(sizeof(Mode43Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode43Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_init(&new_mode->base, interface, &mode_43_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = false;
    new_mode->filter_type = NOTCH_FILTER_R;

    *mode = new_mode;
    log_debug("Mode 43 created successfully");
    return ERROR_NONE;
}