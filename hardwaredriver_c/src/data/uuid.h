#ifndef UUID_H
#define UUID_H

#include <windows.h>
#include <rpc.h>
#include "core/error_codes.h"

// Python-like UUID string length (36 chars + null terminator)
#define UUID_STR_LEN 37

// Generate a UUID4 (random) string, returns ERROR_NONE on success
ErrorCode uuid4(char* out_str, size_t str_len);

// Convert an existing UUID to string, returns ERROR_NONE on success
ErrorCode uuid_str(UUID* uuid, char* out_str, size_t str_len);

#endif