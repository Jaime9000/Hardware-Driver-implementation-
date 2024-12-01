#ifndef MODE_56_H
#define MODE_56_H

#include "mode.h"
#include "serial_interface.h"
#include "error_codes.h"
#include "byte_sync.h"

// Constants
#define MODE_56_READ_SIZE 320
#define MODE_56_INIT_BYTES 32000
#define MODE_56_INIT_IGNORE_COUNT 50
#define MODE_56_BLOCK_SIZE 10  // Uses 10-byte blocks based on pattern
#define MODE_56_MAX_COLLECT 1600

typedef struct {
    Mode base;
    bool is_first_run;
    NotchFilterType filter_type;
} Mode56Raw;

// Constructor/Destructor
ErrorCode mode_56_raw_create(Mode56Raw** mode, SerialInterface* interface);
void mode_56_raw_destroy(Mode56Raw* mode);

// Notch filter variant
ErrorCode mode_56_raw_notch_create(Mode56Raw** mode, SerialInterface* interface, NotchFilterType filter_type);

#endif // MODE_56_H