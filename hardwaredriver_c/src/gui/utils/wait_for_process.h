#ifndef WAIT_FOR_PROCESS_H
#define WAIT_FOR_PROCESS_H

// System headers
#include <stdio.h>
#include <windows.h>

// Project headers
#include "src/core/error_codes.h"

/**
 * @brief Default wait time between process checks in milliseconds
 * @note Equivalent to 0.001 seconds to minimize CPU usage while still being responsive
 */

// Default wait time in milliseconds (equivalent to 0.001 seconds)
#define DEFAULT_WAIT_TIME 1

/**
 * @brief Waits for a process to complete execution
 * 
 * This function polls the process status at specified intervals until
 * the process terminates. If wait_time_ms is 0, DEFAULT_WAIT_TIME is used.
 * 
 * @param process Handle to the process to wait for
 * @param wait_time_ms Time to wait between checks in milliseconds
 * @return ErrorCode ERROR_NONE on success, otherwise:
 *         ERROR_INVALID_PARAM if process handle is invalid
 *         ERROR_PROCESS_CHECK if unable to check process status
 */
ErrorCode wait_process_done(HANDLE process, DWORD wait_time_ms);

#endif // WAIT_FOR_PROCESS_H