#include "src/data/uuid.h"
#include "src/core/error_codes.h"

// System headers
#include <string.h>

ErrorCode uuid4(char* out_str, size_t str_len) {
    if (!out_str || str_len < UUID_STR_LEN) {
        set_last_error(ERROR_INVALID_PARAM);
        return ERROR_INVALID_PARAM;
    }

    UUID uuid;
    RPC_CSTR szUuid = NULL;
    
    // Generate random UUID (equivalent to Python's uuid4())
    if (UuidCreate(&uuid) != RPC_S_OK) {
        set_last_error(ERROR_DATA_INVALID);
        return ERROR_DATA_INVALID;
    }

    // Convert to string
    if (UuidToStringA(&uuid, &szUuid) != RPC_S_OK) {
        set_last_error(ERROR_DATA_INVALID);
        return ERROR_DATA_INVALID;
    }

    // Copy to output buffer
    if (strncpy_s(out_str, str_len, (char*)szUuid, UUID_STR_LEN - 1) != 0) {
        RpcStringFreeA(&szUuid);
        set_last_error(ERROR_BUFF_OVERFLOW);
        return ERROR_BUFF_OVERFLOW;
    }
    out_str[UUID_STR_LEN - 1] = '\0';

    // Free the RPC string
    RpcStringFreeA(&szUuid);
    
    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}

ErrorCode uuid_str(UUID* uuid, char* out_str, size_t str_len) {
    if (!uuid || !out_str || str_len < UUID_STR_LEN) {
        set_last_error(ERROR_INVALID_PARAM);
        return ERROR_INVALID_PARAM;
    }

    RPC_CSTR szUuid = NULL;

    // Convert to string
    if (UuidToStringA(uuid, &szUuid) != RPC_S_OK) {
        set_last_error(ERROR_DATA_INVALID);
        return ERROR_DATA_INVALID;
    }

    // Copy to output buffer
    if (strncpy_s(out_str, str_len, (char*)szUuid, UUID_STR_LEN - 1) != 0) {
        RpcStringFreeA(&szUuid);
        set_last_error(ERROR_BUFF_OVERFLOW);
        return ERROR_BUFF_OVERFLOW;
    }
    out_str[UUID_STR_LEN - 1] = '\0';

    // Free the RPC string
    RpcStringFreeA(&szUuid);

    set_last_error(ERROR_NONE);
    return ERROR_NONE;
}