#ifndef MODE_51_H
#define MODE_51_H

#include "mode_base.h"
#include "serial_interface.h"
#include "error_codes.h"
#include "byte_sync.h"

// Constants
#define MODE_51_READ_SIZE 320
#define MODE_51_INIT_BYTES 32000
#define MODE_51_INIT_IGNORE_COUNT 50
#define MODE_51_BLOCK_SIZE 24  // Changed to match Python (8 CMS + 16 eight-channel)
#define MODE_51_MAX_COLLECT 1600

// Base Mode51 structure
typedef struct {
    ModeBase base;
    bool is_first_run;
} Mode51Raw;

// Constructors for different variants
ErrorCode mode_51_raw_create(Mode51Raw** mode, SerialInterface*serial_interface, ProcessManager* process_manager);
void mode_51_raw_destroy(Mode51Raw* mode);

#endif // MODE_51_H