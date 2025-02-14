#include <stdlib.h>
#include <string.h>
#include "mode_44.h"
#include "logger.h"

static int get_mode_number(const Mode* mode) {
    (void)mode;
    return 44;
}

static const uint8_t* get_emg_config(const Mode* mode, size_t* length) {
    // Use Mode57's EMG config
    return mode_57_get_emg_config(mode, length);
}

static ErrorCode execute(Mode* base, uint8_t* output, size_t* output_length) {
    Mode44Raw* mode = (Mode44Raw*)base->impl;
    
    // Get Mode57's output first
    uint8_t* mode57_buffer = malloc(*output_length);
    if (!mode57_buffer) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    size_t mode57_length = *output_length;
    ErrorCode error = mode_57_raw_execute(&mode->base, mode57_buffer, &mode57_length);
    if (error != ERROR_NONE) {
        free(mode57_buffer);
        return error;
    }

    // Now repeat each 8-byte block 5 times
    size_t output_pos = 0;
    for (size_t i = 0; i < mode57_length; i += 16) {  // Mode57 uses 16-byte blocks
        for (int repeat = 0; repeat < MODE_44_REPEAT_COUNT; repeat++) {
            if (output_pos + MODE_44_BLOCK_SIZE > *output_length) {
                free(mode57_buffer);
                *output_length = output_pos;
                return ERROR_NONE;
            }
            // Copy only first 8 bytes of each 16-byte block
            memcpy(output + output_pos, mode57_buffer + i, MODE_44_BLOCK_SIZE);
            output_pos += MODE_44_BLOCK_SIZE;
        }
    }

    free(mode57_buffer);
    *output_length = output_pos;
    return ERROR_NONE;
}

static ErrorCode execute_no_image(Mode* base, uint8_t* output, size_t* output_length) {
    Mode44RawNoImage* mode = (Mode44RawNoImage*)base->impl;
    
    // Get Mode57NoImage's output first
    uint8_t* mode57_buffer = malloc(*output_length);
    if (!mode57_buffer) {
        return ERROR_MEMORY_ALLOCATION;
    }
    
    size_t mode57_length = *output_length;
    ErrorCode error = mode_57_raw_no_image_execute(&mode->base, mode57_buffer, &mode57_length);
    if (error != ERROR_NONE) {
        free(mode57_buffer);
        return error;
    }

    // Now repeat each 8-byte block 5 times
    size_t output_pos = 0;
    for (size_t i = 0; i < mode57_length; i += 16) {  // Mode57 uses 16-byte blocks
        for (int repeat = 0; repeat < MODE_44_REPEAT_COUNT; repeat++) {
            if (output_pos + MODE_44_BLOCK_SIZE > *output_length) {
                free(mode57_buffer);
                *output_length = output_pos;
                return ERROR_NONE;
            }
            // Copy only first 8 bytes of each 16-byte block
            memcpy(output + output_pos, mode57_buffer + i, MODE_44_BLOCK_SIZE);
            output_pos += MODE_44_BLOCK_SIZE;
        }
    }

    free(mode57_buffer);
    *output_length = output_pos;
    return ERROR_NONE;
}

static const ModeVTable mode_44_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute = execute,
    .execute_not_connected = mode_sweep_execute_not_connected
};

static const ModeVTable mode_44_no_image_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute = execute_no_image,
    .execute_not_connected = mode_sweep_execute_not_connected
};

ErrorCode mode_44_raw_create(Mode44Raw** mode, SerialInterface*serial_interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_44_raw_create");
        return ERROR_INVALID_PARAM;
    }

    Mode44Raw* new_mode = (Mode44Raw*)malloc(sizeof(Mode44Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode44Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    // Initialize as Mode57Raw
    ErrorCode error = mode_57_raw_create((Mode57Raw**)&new_mode,serial_interface);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    // Override only the vtable
    ((Mode*)new_mode)->vtable = &mode_44_vtable;

    *mode = new_mode;
    log_debug("Mode 44 Raw created successfully");
    return ERROR_NONE;
}

void mode_44_raw_destroy(Mode44Raw* mode) {
    if (mode) {
        log_debug("Destroying Mode 44 Raw");
        mode_57_raw_destroy((Mode57Raw*)mode);
    }
}

ErrorCode mode_44_raw_no_image_create(Mode44RawNoImage** mode, SerialInterface*serial_interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_44_raw_no_image_create");
        return ERROR_INVALID_PARAM;
    }

    Mode44RawNoImage* new_mode = (Mode44RawNoImage*)malloc(sizeof(Mode44RawNoImage));
    if (!new_mode) {
        log_error("Failed to allocate Mode44RawNoImage");
        return ERROR_MEMORY_ALLOCATION;
    }

    // Initialize as Mode57RawNoImage
    ErrorCode error = mode_57_raw_no_image_create((Mode57RawNoImage**)&new_mode,serial_interface);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    // Override only the vtable
    ((Mode*)new_mode)->vtable = &mode_44_no_image_vtable;

    *mode = new_mode;
    log_debug("Mode 44 Raw No Image created successfully");
    return ERROR_NONE;
}

void mode_44_raw_no_image_destroy(Mode44RawNoImage* mode) {
    if (mode) {
        log_debug("Destroying Mode 44 Raw No Image");
        mode_57_raw_no_image_destroy((Mode57RawNoImage*)mode);
    }
}