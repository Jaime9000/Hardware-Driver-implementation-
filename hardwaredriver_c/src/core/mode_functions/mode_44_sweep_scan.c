#include <stdlib.h>
#include <string.h>
#include "mode_44_sweep_scan.h"
#include "logger.h"

static int get_mode_number(const Mode* mode) {
    (void)mode;
    return 44;
}

static const uint8_t* get_emg_config(const Mode* mode, size_t* length) {
    (void)mode;  // Mode 44 always uses 'r' config
    *length = 1;
    static const uint8_t config[] = {'r'};
    return config;
}

static const ModeVTable mode_44_sweep_vtable = {
    .get_mode_number = get_mode_number,
    .get_emg_config = get_emg_config,
    .execute = (ExecuteFunc)mode_sweep_execute,  // Use ModeSweep's execute function
    .execute_not_connected = mode_sweep_execute_not_connected
};

ErrorCode mode_44_sweep_create(Mode44Sweep** mode, SerialInterface*serial_interface) {
    if (!mode || !interface) {
        log_error("Invalid parameters in mode_44_sweep_create");
        return ERROR_INVALID_PARAM;
    }

    Mode44Sweep* new_mode = (Mode44Sweep*)malloc(sizeof(Mode44Sweep));
    if (!new_mode) {
        log_error("Failed to allocate Mode44Sweep");
        return ERROR_MEMORY_ALLOCATION;
    }

    // Initialize base ModeSweep with show_sweep_graph=true and show_tilt_window=false
    ErrorCode error = mode_sweep_init(&new_mode->base,serial_interface, &mode_44_sweep_vtable, new_mode, 
                                    /* show_tilt_window= */ false,
                                    /* show_sweep_graph= */ true);
    if (error != ERROR_NONE) {
        free(new_mode);
        return error;
    }

    *mode = new_mode;
    log_debug("Mode 44 Sweep created successfully");
    return ERROR_NONE;
}

void mode_44_sweep_destroy(Mode44Sweep* mode) {
    if (mode) {
        log_debug("Destroying Mode 44 Sweep");
        mode_sweep_destroy(&mode->base);
        free(mode);
    }
}