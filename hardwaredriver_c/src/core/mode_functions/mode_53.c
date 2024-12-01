#include <stdlib.h>
#include <string.h>
#include "mode_53.h"
#include "logger.h"

static int get_mode_number(const Mode* mode) {
    (void)mode;
    return 53;
}

static const uint8_t* get_emg_config(const Mode* mode, size_t* length) {
    (void)mode;  // Mode 53 always uses 'r' config
    *length = 1;
    static const uint8_t config[] = {'r'};
    return config;
}

static bool validate_esg_values(const uint8_t* data) {
    // Check if first byte is 4<<4 and third byte is 5<<4
    return ((data[0] >> 4) == 4) && ((data[2] >> 4) == 5);
}

static ErrorCode execute(Mode* base, uint8_t* output, size_t* output_length) {
    Mode53Raw* mode = (Mode53Raw*)base->impl;
    
    // Handle first run initialization
    if (mode->is_first_run) {
        uint8_t read_buffer[MODE_53_READ_SIZE];
        size_t bytes_read;
        int ignore_count = 0;
        size_t bytes_thrown = 0;

        while (bytes_thrown < MODE_53_INIT_BYTES) {
            ErrorCode error = serial_interface_read(mode->base.interface, 
                                                 read_buffer, MODE_53_READ_SIZE, &bytes_read);
            if (error != ERROR_NONE) continue;

            bytes_thrown += bytes_read;
            
            if (validate_esg_values(read_buffer)) {
                ignore_count++;
            }
            
            if (ignore_count > MODE_53_INIT_IGNORE_COUNT) break;
        }
        
        mode->is_first_run = false;
    }

    // Read and process data
    uint8_t read_buffer[MODE_53_MAX_COLLECT];
    size_t bytes_read;
    
    ErrorCode error = serial_interface_read(mode->base.interface, read_buffer, MODE_53_MAX_COLLECT, &bytes_read);
    if (error != ERROR_NONE) {
        return error;
    }

    // Custom sync implementation for ESG channels
    size_t i = 0;
    size_t final_size = 0;
    bool found_first_set = false;
    uint8_t synced_data[MODE_53_MAX_COLLECT];

    while (i < bytes_read && (bytes_read - i) >= MODE_53_BLOCK_SIZE) {
        if (validate_esg_values(read_buffer + i)) {
            memcpy(synced_data + final_size, read_buffer + i, MODE_53_BLOCK_SIZE);
            final_size += MODE_53_BLOCK_SIZE;
            found_first_set = true;
            i += MODE_53_BLOCK_SIZE;
        } else if (found_first_set) {
            break;
        } else {
            i++;
        }
    }

    if (final_size == 0) {
        log_error("Cannot verify byte order in Mode 53");
        return ERROR_SYNC_FAILED;
    }

    size_t copy_size = final_size;
    if (copy_size > *output_length) {
        copy_size = *output_length;
    }
    
    memcpy(output, synced_data, copy_size);
    *output_length = copy_size;
    return ERROR_NONE;
}

static ErrorCode execute_not_connected(Mode* base, uint8_t* output, size_t* output_length) {
    static const uint8_t pattern[] = {
        4 << 4, 0, 5 << 4, 0
    };

    size_t pattern_size = sizeof(pattern);
    size_t max_repeats = *output_length / pattern_size;
    
    for (size_t i = 0; i < max_repeats; i++) {
        memcpy(output + (i * pattern_size), pattern, pattern_size);
    }
    
    *output_length = max_repeats * pattern_size;
    return ERROR_NONE;
}

static const ModeVTable mode_53_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute = execute,
    .execute_not_connected = execute_not_connected,
    .destroy = NULL
};

ErrorCode mode_53_raw_create(Mode53Raw** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_53_raw_create");
        return ERROR_INVALID_PARAMETER;
    }

    Mode53Raw* new_mode = (Mode53Raw*)malloc(sizeof(Mode53Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode53Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_init(&new_mode->base, interface, &mode_53_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    new_mode->filter_type = NOTCH_FILTER_NONE;

    *mode = new_mode;
    log_debug("Mode 53 Raw created successfully");
    return ERROR_NONE;
}

void mode_53_raw_destroy(Mode53Raw* mode) {
    if (mode) {
        log_debug("Destroying Mode 53 Raw");
        free(mode);
    }
}

ErrorCode mode_53_raw_notch_create(Mode53Raw** mode, SerialInterface* interface, NotchFilterType filter_type) {
    ErrorCode error = mode_53_raw_create(mode, interface);
    if (error != ERROR_NONE) {
        return error;
    }

    (*mode)->filter_type = filter_type;
    return ERROR_NONE;
}