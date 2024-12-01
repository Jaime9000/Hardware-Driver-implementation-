#ifndef MODE_51_H
#define MODE_51_H

#include "mode.h"
#include "serial_interface.h"
#include "error_codes.h"
#include "byte_sync.h"

// Constants
#define MODE_51_READ_SIZE 320
#define MODE_51_INIT_BYTES 32000
#define MODE_51_INIT_IGNORE_COUNT 50
#define MODE_51_BLOCK_SIZE 16
#define MODE_51_MAX_COLLECT 1600

typedef struct {
    Mode base;
    bool is_first_run;
    NotchFilterType filter_type;
} Mode51Raw;

// Constructor/Destructor
ErrorCode mode_51_raw_create(Mode51Raw** mode, SerialInterface* interface);
void mode_51_raw_destroy(Mode51Raw* mode);

// Notch filter variant
ErrorCode mode_51_raw_notch_create(Mode51Raw** mode, SerialInterface* interface, NotchFilterType filter_type);

#endif // MODE_51_H