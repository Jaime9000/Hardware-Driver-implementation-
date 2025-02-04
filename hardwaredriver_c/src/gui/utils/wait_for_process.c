#include "src/gui/utils/wait_for_process.h"
#include "src/core/logger.h"

ErrorCode wait_process_done(HANDLE process, DWORD wait_time_ms) {
    if (!process) {
        log_error("Invalid process handle");
        return ERROR_INVALID_PARAMETER;
    }

    while (TRUE) {
        DWORD exit_code;
        
        // Get process status
        if (!GetExitCodeProcess(process, &exit_code)) {
            log_error("Failed to get process exit code");
            return ERROR_PROCESS_CHECK;
        }

        // Check if process has completed
        if (exit_code != STILL_ACTIVE) {
            break;
        }

        Sleep(wait_time_ms ? wait_time_ms : DEFAULT_WAIT_TIME);
    }

    return ERROR_NONE;
}