#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mode_44.h"
#include "logger.h"

static int get_mode_number(const Mode* mode) {
    (void)mode;
    return 44;
}

static const uint8_t* get_emg_config(const Mode* mode, size_t* length) {
    Mode44Raw* raw_mode = (Mode44Raw*)mode->impl;
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

static bool wait_for_init(const uint8_t* data, size_t length) {
    if (length < MODE_44_BLOCK_SIZE) return false;
    
    SyncResult sync_result;
    ErrorCode error = resync_bytes(data, length, MODE_44_BLOCK_SIZE,
                                 sync_emg_channels, NULL, 0, 0, &sync_result);
    
    bool result = sync_result.found_sync;
    sync_result_free(&sync_result);
    return result;
}

static ErrorCode execute(Mode* base, uint8_t* output, size_t* output_length) {
    Mode44Raw* mode = (Mode44Raw*)base->impl;
    
    // Handle first run initialization
    if (mode->is_first_run) {
        uint8_t read_buffer[MODE_44_READ_SIZE];
        size_t bytes_read;
        int ignore_count = 0;
        size_t bytes_thrown = 0;

        while (bytes_thrown < MODE_44_INIT_THRESHOLD) {
            ErrorCode error = serial_interface_read(mode->base.interface, 
                                                 read_buffer, MODE_44_READ_SIZE, &bytes_read);
            if (error != ERROR_NONE) continue;

            bytes_thrown += bytes_read;
            
            if (wait_for_init(read_buffer, bytes_read)) {
                ignore_count++;
            }
            
            if (ignore_count > MODE_44_INIT_IGNORE_COUNT) break;
        }
        
        mode->is_first_run = false;
    }

    // Read and process data
    uint8_t read_buffer[MODE_44_MAX_COLLECT];
    size_t bytes_read;
    
    ErrorCode error = serial_interface_read(mode->base.interface, read_buffer, MODE_44_MAX_COLLECT, &bytes_read);
    if (error != ERROR_NONE) {
        return error;
    }

    SyncResult sync_result;
    error = resync_bytes(read_buffer, bytes_read, MODE_44_BLOCK_SIZE,
                        sync_emg_channels, NULL, 0, 0, &sync_result);

    if (!sync_result.found_sync) {
        log_error("Cannot verify byte order in Mode 44");
        sync_result_free(&sync_result);
        return ERROR_SYNC_FAILED;
    }

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

static const ModeVTable mode_44_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute = execute,
    .execute_not_connected = execute_not_connected,
    .destroy = NULL
};

ErrorCode mode_44_raw_create(Mode44Raw** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_44_raw_create");
        return ERROR_INVALID_PARAMETER;
    }

    Mode44Raw* new_mode = (Mode44Raw*)malloc(sizeof(Mode44Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode44Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_init(&new_mode->base, interface, &mode_44_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    new_mode->filter_type = NOTCH_FILTER_NONE;

    *mode = new_mode;
    log_debug("Mode 44 Raw created successfully");
    return ERROR_NONE;
}

void mode_44_raw_destroy(Mode44Raw* mode) {
    if (mode) {
        log_debug("Destroying Mode 44 Raw");
        free(mode);
    }
}

ErrorCode mode_44_raw_notch_create(Mode44Raw** mode, SerialInterface* interface, NotchFilterType filter_type) {
    ErrorCode error = mode_44_raw_create(mode, interface);
    if (error != ERROR_NONE) {
        return error;
    }

    (*mode)->filter_type = filter_type;
    return ERROR_NONE;
}