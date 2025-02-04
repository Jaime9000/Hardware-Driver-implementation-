#ifndef UUID_H
#define UUID_H

// System headers
#include <windows.h>
#include <rpc.h>

// Project headers
#include "src/core/error_codes.h"

// Constants
#define UUID_STR_LEN 37  // Python-like UUID string length (36 chars + null terminator)

// Function declarations
/**
 * Generate a UUID4 (random) string
 * @param out_str Output buffer for the UUID string
 * @param str_len Length of the output buffer
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode uuid4(char* out_str, size_t str_len);

/**
 * Convert an existing UUID to string
 * @param uuid Pointer to UUID structure to convert
 * @param out_str Output buffer for the UUID string
 * @param str_len Length of the output buffer
 * @return ERROR_NONE on success, error code otherwise
 */
ErrorCode uuid_str(UUID* uuid, char* out_str, size_t str_len);

#endif // UUID_H