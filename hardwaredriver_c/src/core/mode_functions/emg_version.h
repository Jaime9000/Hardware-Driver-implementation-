#ifndef EMG_VERSION_H
#define EMG_VERSION_H

#include <stdbool.h>
#include <stdint.h>
#include "src/core/mode_functions/mode_base.h"
#include "src/core/serial_interface.h"
#include "src/core/error_codes.h"

#define VERSION_STRING_MAX_LENGTH 8
#define VERSION_DATA_LENGTH 4

typedef struct {
    char version_string[VERSION_STRING_MAX_LENGTH];
} EMGVersionImpl;

// Base EMG Version mode
ErrorCode emg_version_create(ModeBase** mode, SerialInterface* interface, ProcessManager* process_manager);
void emg_version_destroy(ModeBase* mode);

// Hardware connection check variant
ErrorCode hardware_connection_create(ModeBase** mode, SerialInterface* interface, ProcessManager* process_manager);

#endif // EMG_VERSION_H