#include <stdlib.h>
#include <string.h>
#include "mode_57.h"
#include "logger.h"

static int get_mode_number(const Mode* mode) {
    (void)mode;
    return 57;
}

static const uint8_t* get_emg_config(const Mode* mode, size_t* length) {
    (void)mode;  // Mode 57 always uses 'r' config
    *length = 1;
    static const uint8_t config[] = {'r'};
    return config;
}

static const ModeVTable mode_57_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute = (ExecuteFunc)mode_57_raw_execute,
    .execute_not_connected = mode_sweep_execute_not_connected
};

static const ModeVTable mode_57_no_image_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute = (ExecuteFunc)mode_57_raw_no_image_execute,
    .execute_not_connected = mode_sweep_execute_not_connected
};

ErrorCode mode_57_raw_create(Mode57Raw** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_57_raw_create");
        return ERROR_INVALID_PARAM;
    }

    Mode57Raw* new_mode = (Mode57Raw*)malloc(sizeof(Mode57Raw));
    if (!new_mode) {
        log_error("Failed to allocate Mode57Raw");
        return ERROR_MEMORY_ALLOCATION;
    }

    // Initialize base ModeSweep with show_tilt_window=true
    ErrorCode error = mode_sweep_init(&new_mode->base, interface, &mode_57_vtable, new_mode, true);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    *mode = new_mode;
    log_debug("Mode 57 Raw created successfully");
    return ERROR_NONE;
}

void mode_57_raw_destroy(Mode57Raw* mode) {
    if (mode) {
        log_debug("Destroying Mode 57 Raw");
        mode_sweep_destroy(&mode->base);
        free(mode);
    }
}

ErrorCode mode_57_raw_execute(Mode57Raw* mode, uint8_t* output, size_t* output_length) {
    // Mode 57 uses the same execution logic as ModeSweep
    return mode_sweep_execute(&mode->base, output, output_length);
}

ErrorCode mode_57_raw_no_image_create(Mode57RawNoImage** mode, SerialInterface* interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_57_raw_no_image_create");
        return ERROR_INVALID_PARAM;
    }

    Mode57RawNoImage* new_mode = (Mode57RawNoImage*)malloc(sizeof(Mode57RawNoImage));
    if (!new_mode) {
        log_error("Failed to allocate Mode57RawNoImage");
        return ERROR_MEMORY_ALLOCATION;
    }

    // Initialize base ModeSweep with show_tilt_window=false
    ErrorCode error = mode_sweep_init(&new_mode->base, interface, &mode_57_no_image_vtable, new_mode, false);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    new_mode->is_first_run = true;
    *mode = new_mode;
    log_debug("Mode 57 Raw No Image created successfully");
    return ERROR_NONE;
}

void mode_57_raw_no_image_destroy(Mode57RawNoImage* mode) {
    if (mode) {
        log_debug("Destroying Mode 57 Raw No Image");
        mode_sweep_destroy(&mode->base);
        free(mode);
    }
}

ErrorCode mode_57_raw_no_image_execute(Mode57RawNoImage* mode, uint8_t* output, size_t* output_length) {
    // Mode 57 No Image uses the same execution logic as ModeSweep
    return mode_sweep_execute(&mode->base, output, output_length);
}