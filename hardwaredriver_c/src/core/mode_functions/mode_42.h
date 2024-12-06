#ifndef MODE_42_H
#define MODE_42_H

#include "mode_base.h"
#include "mode_43.h"
#include "serial_interface.h"
#include "error_codes.h"
#include "byte_sync.h"

// Constants
#define MODE_42_BLOCK_SIZE 24
#define MODE_42_CMS_SIZE 8
#define MODE_42_EMG_SIZE 16
#define MODE_42_READ_SIZE 320
#define MODE_42_INIT_THRESHOLD 32000
#define MODE_42_INIT_IGNORE_COUNT 50
#define MODE_42_TIMEOUT_MS 60

// Mode types
typedef enum {
    MODE_42_TYPE_RAW,
    MODE_42_TYPE_RAW_EMG,
    MODE_42_TYPE_EQUIPMENT,
    MODE_42_TYPE_LEAD_STATUS,
    MODE_42_TYPE_NOTCH_P,
    MODE_42_TYPE_NOTCH_Q,
    MODE_42_TYPE_NOTCH_R,
    MODE_42_TYPE_NOTCH_S,
    MODE_42_TYPE_NOTCH_T,
    MODE_42_TYPE_NOTCH_U,
    MODE_42_TYPE_NOTCH_V,
    MODE_42_TYPE_NOTCH_W
} Mode42Type;

// Base Mode42 structure
typedef struct {
    ModeBase base;
    bool is_first_run;
    Mode42Type mode_type;
} Mode42;

// Equipment byte mode
typedef struct {
    ModeBase base;
    uint8_t device_byte;
} EquipmentByte;

// EMG Lead Status mode (inherits from Mode43)
typedef struct {
    Mode43 base43;
    ModeBase* mode42_base;
} GetEMGLeadStatus;

// Constructors for different variants
ErrorCode mode_42_raw_create(Mode42** mode, SerialInterface* interface, ProcessManager* process_manager);
ErrorCode mode_42_raw_emg_create(Mode42** mode, SerialInterface* interface, ProcessManager* process_manager);
ErrorCode mode_42_equipment_create(EquipmentByte** mode, SerialInterface* interface, ProcessManager* process_manager);
ErrorCode mode_42_lead_status_create(GetEMGLeadStatus** mode, SerialInterface* interface, ProcessManager* process_manager);
ErrorCode mode_42_raw_notch_create(Mode42** mode, SerialInterface* interface, ProcessManager* process_manager, Mode42Type notch_type);

// Destructors
void mode_42_destroy(Mode42* mode);
void equipment_byte_destroy(EquipmentByte* mode);
void emg_lead_status_destroy(GetEMGLeadStatus* mode);

#endif // MODE_42_H