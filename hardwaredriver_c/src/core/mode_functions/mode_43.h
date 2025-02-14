#ifndef MODE_43_H
#define MODE_43_H

#include "mode_base.h"
#include "serial_interface.h"
#include "error_codes.h"
#include "byte_sync.h"

// Constants
#define MODE_43_READ_SIZE 320
#define MODE_43_INIT_BYTES 16000
#define MODE_43_MAX_COLLECT 1700
#define MODE_43_TIMEOUT_MS 60
#define MODE_43_BLOCK_SIZE 16

// EMG Mode types
typedef enum {
    MODE_43_TYPE_RAW,
    MODE_43_TYPE_RAW_EMG,
    MODE_43_TYPE_NOTCH_P,
    MODE_43_TYPE_NOTCH_Q,
    MODE_43_TYPE_NOTCH_R,
    MODE_43_TYPE_NOTCH_S,
    MODE_43_TYPE_NOTCH_T,
    MODE_43_TYPE_NOTCH_U,
    MODE_43_TYPE_NOTCH_V,
    MODE_43_TYPE_NOTCH_W
} Mode43Type;

typedef struct {
    ModeBase base;
    bool is_first_run;
    Mode43Type mode_type;
} Mode43;
/*
*implementaion of load_saved_data() from Python implementation still needed, this uses pickle,
* but we can use our serial_deserialize() implementaiton to acheive the same functionality.
*

*/
// Constructors for different variants
ErrorCode mode_43_raw_create(Mode43** mode, SerialInterface*serial_interface, ProcessManager* process_manager);
ErrorCode mode_43_raw_emg_create(Mode43** mode, SerialInterface*serial_interface, ProcessManager* process_manager);
ErrorCode mode_43_raw_notch_create(Mode43** mode, SerialInterface*serial_interface, ProcessManager* process_manager, Mode43Type notch_type);

// Destructor
void mode_43_destroy(Mode43* mode);

#endif // MODE_43_H