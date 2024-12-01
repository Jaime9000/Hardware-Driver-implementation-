#include <stdlib.h>
#include <string.h>
#include "mode_56.h"
#include "logger.h"

static int get_mode_number(const Mode* mode) {
    (void)mode;
    return 56;
}

static const uint8_t* get_emg_config(const Mode* mode, size_t* length) {
    (void)mode;  // Mode 56 always uses 'r' config
    *length = 1;
    static const uint8_t config[] = {'r'};
    return config;
}

static ErrorCode execute(Mode* base, uint8_t* output, size_t* output_length) {
    Mode56Raw* mode = (Mode56Raw*)base->impl;
    
    // Handle first run initialization
    if (mode->is_first_run) {
        uint8_t read_buffer[MODE_56_READ_SIZE];
        size_t bytes_read;
        int ignore_count = 0;
        size_t bytes_thrown = 0;

        while (bytes_thrown < MODE_56_INIT_BYTES) {
            ErrorCode error = serial_interface_read(mode->base.interface, 
                                                 read_buffer, MODE_56_READ_SIZE, &bytes_read);
            if (error != ERROR_NONE) continue;

            bytes_thrown += bytes_read;
            ignore_count++;
            
            if (ignore_count > MODE_56_INIT_IGNORE_COUNT) break;
        }
        
        mode->is_first_run = false;
    }

    // Read and process data
    uint8_t read_buffer[MODE_56_MAX_COLLECT];
    size_t bytes_read;
    
    ErrorCode error = serial_interface_read(mode->base.interface, read_buffer, MODE_56_MAX_COLLECT, &bytes_read);
    if (error != ERROR_NONE) {
        return error;
    }

    // Copy data directly as Mode56 doesn't require special syncing
    size_t copy_size = bytes_read;
    if (copy_size > *output_length) {
        copy_size = *output_length;
    }
    
    memcpy(output, read_buffer, copy_size);
    *output_length = copy_size;
    return ERROR_NONE;
}

static ErrorCode execute_not_connected(Mode* base, uint8_t* output, size_t* output_length) {
    static const uint8_t pattern[] = {
        7, 204, 23, 69, 55, 192, 72, 12, 88, 9
    };

    size_t pattern_size = sizeof(pattern);
    size_t max_repeats = *output_length / pattern_size;
    
    for (size_t i = 0; i < max_repeats; i++) {
        memcpy(output + (i * pattern_size), pattern, pattern_size);
    }
    
    *output_length = max_repeats * pattern_size;
    return ERROR_NONE;
}

static const ModeVTable mode_56_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute = execute,
    .execute_not_connected = execute_not_connected,
    .destroy = NULL
};

ErrorCode mode_56_raw_create(Mode56Raw** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_56_raw_create");
        return ERROR_INVALID_PARAMETER;
    }

    Mode56Raw* new_mode = (Mode56Raw*)malloc(sizeof(Mode56Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode56Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_init(&new_mode->base, interface, &mode_56_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    new_mode->filter_type = NOTCH_FILTER_NONE;

    *mode = new_mode;
    log_debug("Mode 56 Raw created successfully");
    return ERROR_NONE;
}

void mode_56_raw_destroy(Mode56Raw* mode) {
    if (mode) {
        log_debug("Destroying Mode 56 Raw");
        free(mode);
    }
}

ErrorCode mode_56_raw_notch_create(Mode56Raw** mode, SerialInterface* interface, NotchFilterType filter_type) {
    ErrorCode error = mode_56_raw_create(mode, interface);
    if (error != ERROR_NONE) {
        return error;
    }

    (*mode)->filter_type = filter_type;
    return ERROR_NONE;
}