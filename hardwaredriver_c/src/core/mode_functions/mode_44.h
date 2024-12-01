#ifndef MODE_44_H
#define MODE_44_H

#include "mode.h"
#include "serial_interface.h"
#include "error_codes.h"
#include "byte_sync.h"

// Constants
#define MODE_44_READ_SIZE 320
#define MODE_44_INIT_THRESHOLD 32000
#define MODE_44_INIT_IGNORE_COUNT 50
#define MODE_44_BLOCK_SIZE 16
#define MODE_44_MAX_COLLECT 1600

typedef struct {
    Mode base;
    bool is_first_run;
    NotchFilterType filter_type;
} Mode44Raw;

// Constructor/Destructor
ErrorCode mode_44_raw_create(Mode44Raw** mode, SerialInterface* interface);
void mode_44_raw_destroy(Mode44Raw* mode);

// Notch filter variant
ErrorCode mode_44_raw_notch_create(Mode44Raw** mode, SerialInterface* interface, NotchFilterType filter_type);

#endif // MODE_44_H