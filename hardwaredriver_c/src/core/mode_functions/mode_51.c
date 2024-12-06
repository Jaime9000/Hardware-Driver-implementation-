#include <stdlib.h>
#include <string.h>
#include "mode_51.h"
#include "logger.h"

static int get_mode_number(const ModeBase* mode) {
    (void)mode;
    return 51;
}

static const uint8_t* get_emg_config(const ModeBase* mode, size_t* length) {
    static const uint8_t config[] = {'r'};
    *length = 1;
    return config;
}

static ErrorCode execute_mode(ModeBase* base, uint8_t* output, size_t* output_length) {
    if (!base || !output || !output_length) {
        return ERROR_INVALID_PARAMETER;
    }

    uint8_t raw_data[MODE_51_MAX_COLLECT];
    size_t bytes_read;
    
    ErrorCode error = serial_interface_read(base->interface, raw_data, MODE_51_MAX_COLLECT, &bytes_read);
    if (error != ERROR_NONE) {
        return error;
    }

    SyncResult sync_result;
    error = resync_bytes(raw_data, bytes_read, MODE_51_BLOCK_SIZE,
                        sync_cms_channels, sync_8_channels,
                        0, 8, &sync_result);  // CMS at offset 0, 8-channel at offset 8

    if (!sync_result.found_sync) {
        log_error("Cannot verify byte order in Mode 51");
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
        return ERROR_INVALID_PARAMETER;
    }

    // Match Python's pattern: [0, 0, 1<<4, 0, 2<<4, 0, 3<<4, 0] + [0, 0, 1<<4, 0, 2<<4, 0, 3<<4, 0, 4<<4, 0, 5<<4, 0, 6<<4, 0, 7<<4, 0]
    static const uint8_t pattern[] = {
        0, 0, 0x10, 0, 0x20, 0, 0x30, 0,  // CMS channels
        0, 0, 0x10, 0, 0x20, 0, 0x30, 0,  // First half of 8 channels
        0x40, 0, 0x50, 0, 0x60, 0, 0x70, 0   // Second half of 8 channels
    };

    size_t pattern_size = sizeof(pattern);
    size_t max_repeats = *output_length / pattern_size;
    
    for (size_t i = 0; i < max_repeats; i++) {
        memcpy(output + (i * pattern_size), pattern, pattern_size);
    }
    
    *output_length = max_repeats * pattern_size;
    return ERROR_NONE;
}

static const ModeBaseVTable mode_51_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute_mode = execute_mode,
    .execute_mode_not_connected = execute_mode_not_connected,
    .stop = NULL,
    .destroy = NULL
};

ErrorCode mode_51_raw_create(Mode51Raw** mode, SerialInterface* interface, ProcessManager* process_manager) {
    if (!mode || !interface || !process_manager) {
        log_error("Invalid parameters in mode_51_raw_create");
        return ERROR_INVALID_PARAMETER;
    }

    Mode51Raw* new_mode = (Mode51Raw*)malloc(sizeof(Mode51Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode51Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    ErrorCode error = mode_base_create(&new_mode->base, interface, process_manager,
                                     &mode_51_vtable, new_mode);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;

    *mode = new_mode;
    log_debug("Mode 51 Raw created successfully");
    return ERROR_NONE;
}

void mode_51_raw_destroy(Mode51Raw* mode) {
    if (mode) {
        mode_base_destroy(&mode->base);
        free(mode);
        log_debug("Mode 51 Raw destroyed");
    }
}