#ifndef MODE_43_H
#define MODE_43_H

#include "mode.h"
#include "serial_interface.h"
#include "error_codes.h"
#include "byte_sync.h"

// Constants
#define MODE_43_READ_SIZE 320
#define MODE_43_INIT_BYTES 16000
#define MODE_43_MAX_COLLECT 1700
#define MODE_43_TIMEOUT_MS 60
#define MODE_43_BLOCK_SIZE 16  // EMG block size

typedef struct {
    Mode base;
    bool is_first_run;
    NotchFilterType filter_type;
} Mode43Raw;

// Constructor/Destructor
ErrorCode mode_43_raw_create(Mode43Raw** mode, SerialInterface* interface);
void mode_43_raw_destroy(Mode43Raw* mode);

// Notch filter variant
ErrorCode mode_43_raw_notch_create(Mode43Raw** mode, SerialInterface* interface, NotchFilterType filter_type);

#endif // MODE_43_H