#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <time.h>
#include "error_codes.h"

#define MAX_HASH_LENGTH 33  // MD5 hex string (32 chars + null terminator)
#define MAX_DATETIME_LENGTH 32

// Function declarations
ErrorCode encode_name(const char* patient_name, char* output, size_t output_size);
ErrorCode encode_curr_datetime(char* output, size_t output_size);
ErrorCode decode_encoded_datetime(const char* encoded_datetime, struct tm* decoded_time);

#endif // UTILS_H