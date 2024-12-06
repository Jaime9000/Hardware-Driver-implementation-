#ifndef MODE_0_H
#define MODE_0_H

#include <stdbool.h>
#include <stdint.h>
#include "mode_base.h"
#include "byte_sync.h"
#include "error_codes.h"

// Constants
#define MODE_0_CHANNEL_COUNT 4
#define MODE_0_BLOCK_SIZE 8
#define MODE_0_READ_SIZE 160
#define MODE_0_DEFAULT_ALIGN_VALUE 2048
#define MODE_0_LATERAL_CHANNEL_INDEX 2

// Mode 0 implementation structure
typedef struct {
    ModeBase base;
    int16_t align_values[MODE_0_CHANNEL_COUNT];
    int16_t offset_values[MODE_0_CHANNEL_COUNT];
    int16_t prev_data_array[MODE_0_CHANNEL_COUNT];
    bool has_prev_data;
    bool is_first_run;
} Mode0;

typedef struct {
    Mode0 base;
    int16_t current_aligned_values[MODE_0_CHANNEL_COUNT];
    bool has_aligned_values;
} Mode0Align;

// Constructor/Destructor for Mode0Align
ErrorCode mode_0_align_create(Mode0Align** mode, SerialInterface* interface, ProcessManager* process_manager);
void mode_0_align_destroy(Mode0Align* mode);


// Constructor/Destructor
ErrorCode mode_0_create(Mode0** mode, SerialInterface* interface, ProcessManager* process_manager);
void mode_0_destroy(Mode0* mode);

/* NO NEED FOR specific __validate_values() function in C implementation:
 *
 * Validates CMS channel synchronization by checking the high nibble pattern.
 * 
 * This function implements the same validation logic as Python's __validate_values:
 *   @staticmethod
 *   def __validate_values(values):
 *       for iteration, index in enumerate(range(0, 8, 2)):
 *           if iteration != values[index] >> 4:
 *               return False
 *       return True
 * 
 * The validation ensures that:
 * - Position 0: high nibble = 0  (0000 xxxx)
 * - Position 2: high nibble = 1  (0001 xxxx)
 * - Position 4: high nibble = 2  (0010 xxxx)
 * - Position 6: high nibble = 3  (0011 xxxx)
 * 
 * This pattern validation is used by Mode0 through resync_bytes():
 *   resync_bytes(data, length, MODE_0_BLOCK_SIZE, sync_cms_channels, NULL, 0, 0, &result)
 * 
 * By using sync_cms_channels as the validation function in resync_bytes, we achieve
 * the same functionality as Python's __validate_values without needing a separate
 * implementation in mode_0.c
 */

 /*
 * Generic byte synchronization function that matches and extends the Python implementation.
 * 
 * This function implements the same core logic as Python's _re_sync_bytes method from mode_0.py:
 * - Searches for valid sync patterns in the data stream
 * - Collects blocks of synchronized data
 * - Stops when sync is lost after finding first valid block
 * 
 * While the Python implementation is specific to CMS channels:
 *   @classmethod
 *   def _re_sync_bytes(cls, values_bucket: List):
 *       i = 0
 *       final_values = []
 *       found_first_set = False
 *       while i < len(values_bucket) and len(values_bucket[i:]) >= 8:
 *           cms_is_valid = re_sync_values_cms_channels(values_bucket[i:i + 8])
 *           if cms_is_valid:
 *               final_values.extend(values_bucket[i:i + 8])
 *               found_first_set = True
 *               i += 8
 *           elif found_first_set:
 *               break
 *           else:
 *               i += 1
 * 
 * This C implementation is more generic and flexible:
 * - Supports two sync functions (with sync_func2 being optional)
 * - Configurable sync offsets
 * - Adjustable block sizes
 * 
 * When used for Mode 0 (CMS channels), it's called with:
 *   resync_bytes(data, length, MODE_0_BLOCK_SIZE, sync_cms_channels, NULL, 0, 0, &result)
 * Which effectively matches the Python implementation's behavior.
 * 
 * Parameters:
 *   data          - Input byte array to synchronize
 *   length        - Length of input data
 *   block_size    - Size of each synchronized block
 *   sync_func1    - Primary sync validation function
 *   sync_func2    - Secondary sync validation function (optional, NULL if not used)
 *   sync1_offset  - Offset for first sync function
 *   sync2_offset  - Offset for second sync function
 *   result        - Output structure containing synchronized data
 * 
 * Returns:
 *   ErrorCode indicating success or specific failure condition
 */
// Mode 0 specific functions
ErrorCode mode_0_process_values(Mode0* mode, const uint8_t* values, int16_t* data_array);
ErrorCode mode_0_execute(Mode0* mode, uint8_t* output, size_t* output_length);
ErrorCode mode_0_execute_not_connected(Mode0* mode, uint8_t* output, size_t* output_length);

// Raw mode variant
typedef struct {
    Mode0 base;
    bool is_first_run;
} Mode0Raw;

ErrorCode mode_0_raw_create(Mode0Raw** mode, SerialInterface* interface, ProcessManager* process_manager);
void mode_0_raw_destroy(Mode0Raw* mode);
ErrorCode mode_0_raw_execute(Mode0Raw* mode, uint8_t* output, size_t* output_length);

#endif // MODE_0_H