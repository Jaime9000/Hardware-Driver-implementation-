#ifndef EMG_VERSION_H
#define EMG_VERSION_H

#include <stdbool.h>
#include <stdint.h>
#include "mode.h"
#include "serial_interface.h"
#include "error_codes.h"

#define VERSION_STRING_MAX_LENGTH 8
#define VERSION_DATA_LENGTH 4

typedef struct {
    Mode base;
    char version_string[VERSION_STRING_MAX_LENGTH];
} EMGVersionAdsOn;

// Base EMG Version mode
ErrorCode emg_version_create(EMGVersionAdsOn** mode, SerialInterface* interface);
void emg_version_destroy(EMGVersionAdsOn* mode);

// Hardware connection check variant
typedef struct {
    EMGVersionAdsOn base;
} CheckHardwareConnection;

ErrorCode hardware_connection_create(CheckHardwareConnection** mode, SerialInterface* interface);
void hardware_connection_destroy(CheckHardwareConnection* mode);

#endif // EMG_VERSION_H