#ifndef MODE_53_H
#define MODE_53_H

#include "mode_base.h"
#include "serial_interface.h"
#include "error_codes.h"
#include "byte_sync.h"

// Constants
#define MODE_53_READ_SIZE 320
#define MODE_53_INIT_BYTES 32000
#define MODE_53_INIT_IGNORE_COUNT 50
#define MODE_53_BLOCK_SIZE 4  // Uses 4-byte blocks for ESG channels
#define MODE_53_MAX_COLLECT 1600

typedef struct {
    ModeBase base;
    bool is_first_run;
} Mode53Raw;

// Constructor/Destructor
ErrorCode mode_53_raw_create(Mode53Raw** mode, SerialInterface* interface, ProcessManager* process_manager);
void mode_53_raw_destroy(Mode53Raw* mode);

#endif // MODE_53_H