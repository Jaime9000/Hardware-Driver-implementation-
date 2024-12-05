#ifndef WAIT_FOR_PROCESS_H
#define WAIT_FOR_PROCESS_H

#include <windows.h>
#include "error_codes.h"

// Default wait time in milliseconds (equivalent to 0.001 seconds)
#define DEFAULT_WAIT_TIME 1

ErrorCode wait_process_done(HANDLE process, DWORD wait_time_ms);

#endif // WAIT_FOR_PROCESS_H