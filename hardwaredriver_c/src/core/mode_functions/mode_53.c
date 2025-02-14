#include <stdlib.h>
#include <string.h>
#include "mode_53.h"
#include "logger.h"

static int get_mode_number(const ModeBase* mode) {
    (void)mode;
    return 53;
}

static const uint8_t* get_emg_config(const ModeBase* mode, size_t* length) {
    (void)mode;  // Mode 53 always uses 'r' config
    static const uint8_t config[] = {'r'};
    *length = 1;
    return config;
}

static ErrorCode execute_mode(ModeBase* base, uint8_t* output, size_t* output_length) {
    if (!base || !output || !output_length) {
        return ERROR_INVALID_PARAM;
    }

    // Read and process data
    uint8_t raw_data[MODE_53_MAX_COLLECT];
    size_t bytes_read;
    
    ErrorCode error = serial_interface_read_data(base->interface, raw_data, MODE_53_MAX_COLLECT, &bytes_read);
    if (error != ERROR_NONE) {
        return error;
    }

    SyncResult sync_result;
    error = resync_bytes(raw_data, bytes_read, MODE_53_BLOCK_SIZE,
                        sync_esg_channels, NULL,
                        0, 0, &sync_result);

    if (!sync_result.found_sync) {
        log_error("Cannot verify byte order in Mode 53");
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

static ErrorCode execute_mode_not_connected(ModeBase* base, uint8_t* output, size_t* output_length) {
    if (!output || !output_length) {
        return ERROR_INVALID_PARAM;
    }

    // Match Python's pattern: [4<<4, 0, 5<<4, 0]
    static const uint8_t pattern[] = {
        0x40, 0x00, 0x50, 0x00
    };

    size_t pattern_size = sizeof(pattern);
    size_t max_repeats = *output_length / pattern_size;
    
    for (size_t i = 0; i < max_repeats; i++) {
        memcpy(output + (i * pattern_size), pattern, pattern_size);
    }
    
    *output_length = max_repeats * pattern_size;
    return ERROR_NONE;
}

static const ModeBaseVTable mode_53_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute_mode = execute_mode,
    .execute_mode_not_connected = execute_mode_not_connected,
    .stop = NULL,
    .destroy = NULL
};

ErrorCode mode_53_raw_create(Mode53Raw** mode, SerialInterface*serial_interface, ProcessManager* process_manager) {
    if (!mode || !interface || !process_manager) {
        log_error("Invalid parameters in mode_53_raw_create");
        return ERROR_INVALID_PARAM;
    }

    Mode53Raw* new_mode = (Mode53Raw*)malloc(sizeof(Mode53Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode53Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_base_create(&new_mode->base,serial_interface, process_manager,
                                     &mode_53_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;

    *mode = new_mode;
    log_debug("Mode 53 Raw created successfully");
    return ERROR_NONE;
}

void mode_53_raw_destroy(Mode53Raw* mode) {
    if (mode) {
        mode_base_destroy(&mode->base);
        free(mode);
        log_debug("Mode 53 Raw destroyed");
    }
}