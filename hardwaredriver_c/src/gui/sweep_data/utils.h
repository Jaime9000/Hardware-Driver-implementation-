#ifndef UTILS_H
#define UTILS_H

// System headers
#include <windows.h>

// Project headers
#include "src/core/error_codes.h"

// Constants
#define MAX_HASH_LENGTH 33       // MD5 hex string (32 chars + null terminator)
#define MAX_DATETIME_LENGTH 32   // Maximum length for datetime string

/**
 * @brief Encodes a patient name using MD5 hashing
 * 
 * @param patient_name Input patient name to encode
 * @param output Buffer to store the encoded hash string
 * @param output_size Size of the output buffer (must be >= MAX_HASH_LENGTH)
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode encode_name(const char* patient_name, char* output, size_t output_size);

/**
 * @brief Encodes current datetime in ISO format with custom separator
 * Format: YYYY-MM-DDTHH__MM__SS.mmmmmm
 * 
 * @param output Buffer to store the encoded datetime string
 * @param output_size Size of the output buffer (must be >= MAX_DATETIME_LENGTH)
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode encode_curr_datetime(char* output, size_t output_size);

/**
 * @brief Decodes an encoded datetime string back to SYSTEMTIME
 * 
 * @param encoded_datetime Input datetime string in format YYYY-MM-DDTHH__MM__SS.mmmmmm
 * @param decoded_time Pointer to SYSTEMTIME structure to store decoded time
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
ErrorCode decode_encoded_datetime(const char* encoded_datetime, SYSTEMTIME* decoded_time);

#endif // UTILS_H