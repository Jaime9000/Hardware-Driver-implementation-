#include <stdlib.h>
#include <string.h>
#include "mode_52.h"
#include "logger.h"

static int get_mode_number(const ModeBase* mode) {
    (void)mode;
    return 52;
}

static const uint8_t* get_emg_config(const ModeBase* mode, size_t* length) {
    (void)mode;  // Mode 52 always uses 'r' config
    static const uint8_t config[] = {'r'};
    *length = 1;
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

static ErrorCode execute_mode(ModeBase* base, uint8_t* output, size_t* output_length) {
    if (!base || !output || !output_length) {
        return ERROR_INVALID_PARAM;
    }
    
    // Read and process data
    uint8_t read_buffer[MODE_52_MAX_COLLECT];
    size_t bytes_read;
    
    ErrorCode error = serial_interface_read_data(base->interface, read_buffer, &bytes_read, MODE_52_MAX_COLLECT);
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

static ErrorCode execute_mode_not_connected(ModeBase* base, uint8_t* output, size_t* output_length) {
    if (!output || !output_length) {
        return ERROR_INVALID_PARAM;
    }

    // Match Python's pattern: [0, 0, 3<<4, 0, 4<<4, 0, 5<<4, 0]
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

static const ModeBaseVTable mode_52_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute_mode = execute_mode,
    .execute_mode_not_connected = execute_mode_not_connected,
    .stop = NULL,
    .destroy = NULL
};

ErrorCode mode_52_raw_create(Mode52Raw** mode, SerialInterface*serial_interface, ProcessManager* process_manager) {
    if (!mode || !interface || !process_manager) {
        log_error("Invalid parameters in mode_52_raw_create");
        return ERROR_INVALID_PARAM;
    }

    Mode52Raw* new_mode = (Mode52Raw*)malloc(sizeof(Mode52Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode52Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_base_create(&new_mode->base,serial_interface, process_manager,
                                     &mode_52_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;

    *mode = new_mode;
    log_debug("Mode 52 Raw created successfully");
    return ERROR_NONE;
}

void mode_52_raw_destroy(Mode52Raw* mode) {
    if (mode) {
        mode_base_destroy(&mode->base);
        free(mode);
        log_debug("Mode 52 Raw destroyed");
    }
}