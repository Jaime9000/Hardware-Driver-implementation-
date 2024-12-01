#ifndef MODE_52_H
#define MODE_52_H

#include "mode.h"
#include "serial_interface.h"
#include "error_codes.h"
#include "byte_sync.h"

// Constants
#define MODE_52_READ_SIZE 320
#define MODE_52_INIT_BYTES 32000
#define MODE_52_INIT_IGNORE_COUNT 50
#define MODE_52_BLOCK_SIZE 8  // Different from other modes - uses 8-byte blocks
#define MODE_52_MAX_COLLECT 1600

typedef struct {
    Mode base;
    bool is_first_run;
    NotchFilterType filter_type;
} Mode52Raw;

// Constructor/Destructor
ErrorCode mode_52_raw_create(Mode52Raw** mode, SerialInterface* interface);
void mode_52_raw_destroy(Mode52Raw* mode);

// Notch filter variant
ErrorCode mode_52_raw_notch_create(Mode52Raw** mode, SerialInterface* interface, NotchFilterType filter_type);

#endif // MODE_52_H