#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mode_43.h"
#include "logger.h"

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
    
    // Handle first run initialization
    if (!mode->is_first_run) {
        uint8_t temp_buffer[MODE_43_READ_SIZE];
        size_t bytes_thrown = 0;
        
        while (bytes_thrown < MODE_43_INIT_BYTES) {
            size_t bytes_read;
            ErrorCode error = serial_interface_read(mode->base.interface, 
                                                  temp_buffer, 
                                                  MODE_43_READ_SIZE, 
                                                  &bytes_read);
            if (error != ERROR_NONE) return error;
            bytes_thrown += bytes_read;
        }
        mode->is_first_run = true;
    }

    // Read and process data
    uint8_t* data_collected = malloc(MODE_43_MAX_COLLECT);
    if (!data_collected) return ERROR_MEMORY_ALLOCATION;
    
    size_t total_collected = 0;
    clock_t start_time = clock();
    
    while (total_collected < MODE_43_MAX_COLLECT) {
        size_t bytes_read;
        ErrorCode error = serial_interface_read(mode->base.interface, 
                                              data_collected + total_collected,
                                              MODE_43_READ_SIZE, 
                                              &bytes_read);
        if (error != ERROR_NONE) {
            free(data_collected);
            return error;
        }
        
        total_collected += bytes_read;
        
        clock_t current_time = clock();
        if (((current_time - start_time) * 1000.0) / CLOCKS_PER_SEC > MODE_43_TIMEOUT_MS) {
            break;
        }
    }

    // Use byte_sync.h functionality for resyncing
    SyncResult sync_result;
    ErrorCode error = resync_bytes(data_collected, total_collected, MODE_43_BLOCK_SIZE,
                                 sync_emg_channels, NULL, 0, 0, &sync_result);
    
    free(data_collected);

    if (!sync_result.found_sync) {
        sync_result_free(&sync_result);
        return ERROR_SYNC_FAILED;
    }

    // Copy to output buffer
    size_t copy_size = sync_result.synced_length;
    if (copy_size > *output_length) {
        copy_size = *output_length;
    }
    
    memcpy(output, sync_result.synced_data, copy_size);
    *output_length = copy_size;

    sync_result_free(&sync_result);
    return ERROR_NONE;
}

static ErrorCode execute_not_connected(Mode* base, uint8_t* output, size_t* output_length) {
    // Generate simulated data pattern for EMG channels
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
    new_mode->filter_type = NOTCH_FILTER_NONE;

    *mode = new_mode;
    log_debug("Mode 43 Raw created successfully");
    return ERROR_NONE;
}

void mode_43_raw_destroy(Mode43Raw* mode) {
    if (mode) {
        log_debug("Destroying Mode 43 Raw");
        free(mode);
    }
}

ErrorCode mode_43_raw_notch_create(Mode43Raw** mode, SerialInterface* interface, NotchFilterType filter_type) {
    ErrorCode error = mode_43_raw_create(mode, interface);
    if (error != ERROR_NONE) {
        return error;
    }

    (*mode)->filter_type = filter_type;
    return ERROR_NONE;
}