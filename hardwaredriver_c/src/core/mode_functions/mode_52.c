#include <stdlib.h>
#include <string.h>
#include "mode_52.h"
#include "logger.h"

static int get_mode_number(const Mode* mode) {
    (void)mode;
    return 52;
}

static const uint8_t* get_emg_config(const Mode* mode, size_t* length) {
    (void)mode;  // Mode 52 always uses 'r' config
    *length = 1;
    static const uint8_t config[] = {'r'};
    return config;
}

static bool validate_values(const uint8_t* data) {
    static const uint8_t channels[] = {0, 3, 4, 5};
    
    for (size_t i = 0; i < sizeof(channels); i++) {
        if (channels[i] != (data[i * 2] >> 4)) {
            return false;
        }
    }
    return true;
}

static ErrorCode execute(Mode* base, uint8_t* output, size_t* output_length) {
    Mode52Raw* mode = (Mode52Raw*)base->impl;
    
    // Read and process data
    uint8_t read_buffer[MODE_52_MAX_COLLECT];
    size_t bytes_read;
    
    ErrorCode error = serial_interface_read(mode->base.interface, read_buffer, MODE_52_MAX_COLLECT, &bytes_read);
    if (error != ERROR_NONE) {
        return error;
    }

    // Custom sync implementation for Mode 52
    size_t i = 0;
    size_t final_size = 0;
    bool found_first_set = false;
    uint8_t synced_data[MODE_52_MAX_COLLECT];

    while (i < bytes_read && (bytes_read - i) >= MODE_52_BLOCK_SIZE) {
        if (validate_values(read_buffer + i)) {
            memcpy(synced_data + final_size, read_buffer + i, MODE_52_BLOCK_SIZE);
            final_size += MODE_52_BLOCK_SIZE;
            found_first_set = true;
            i += MODE_52_BLOCK_SIZE;
        } else if (found_first_set) {
            break;
        } else {
            i++;
        }
    }

    if (final_size == 0) {
        log_error("Cannot verify byte order in Mode 52");
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
        0x00, 0x00, 0x30, 0x00, 0x40, 0x00, 0x50, 0x00
    };

    size_t pattern_size = sizeof(pattern);
    size_t max_repeats = *output_length / pattern_size;
    
    for (size_t i = 0; i < max_repeats; i++) {
        memcpy(output + (i * pattern_size), pattern, pattern_size);
    }
    
    *output_length = max_repeats * pattern_size;
    return ERROR_NONE;
}

static const ModeVTable mode_52_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute = execute,
    .execute_not_connected = execute_not_connected,
    .destroy = NULL
};

ErrorCode mode_52_raw_create(Mode52Raw** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_52_raw_create");
        return ERROR_INVALID_PARAMETER;
    }

    Mode52Raw* new_mode = (Mode52Raw*)malloc(sizeof(Mode52Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode52Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_init(&new_mode->base, interface, &mode_52_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    new_mode->filter_type = NOTCH_FILTER_NONE;

    *mode = new_mode;
    log_debug("Mode 52 Raw created successfully");
    return ERROR_NONE;
}

void mode_52_raw_destroy(Mode52Raw* mode) {
    if (mode) {
        log_debug("Destroying Mode 52 Raw");
        free(mode);
    }
}

ErrorCode mode_52_raw_notch_create(Mode52Raw** mode, SerialInterface* interface, NotchFilterType filter_type) {
    ErrorCode error = mode_52_raw_create(mode, interface);
    if (error != ERROR_NONE) {
        return error;
    }

    (*mode)->filter_type = filter_type;
    return ERROR_NONE;
}