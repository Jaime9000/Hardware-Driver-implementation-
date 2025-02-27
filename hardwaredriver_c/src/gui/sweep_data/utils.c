#include "src/gui/sweep_data/utils.h"
#include "src/core/logger.h"

// System headers
#include <stdio.h>
#include <string.h>
#include <bcrypt.h>
#include <windows.h>
#include <ntstatus.h>  // For STATUS_SUCCESS

#pragma comment(lib, "bcrypt.lib")

// Constants for BCrypt
#define MD5_HASH_LENGTH 16  // MD5 hash length in bytes
#define HEX_CHAR_LENGTH 2   // Length of hex representation per byte

/**
 * @brief Encodes a patient name using MD5 hashing
 */
ErrorCode encode_name(const char* patient_name, char* output, size_t output_size) {
    if (!patient_name || !output || output_size < MAX_HASH_LENGTH) {
        return ERROR_INVALID_PARAM;
    }

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    DWORD hash_length = 0;
    DWORD data_length = 0;
    PBYTE hash_object = NULL;
    PBYTE hash_data = NULL;
    ErrorCode result = ERROR_NONE;

    // Initialize BCrypt (open algorithm provider)
    status = BCryptOpenAlgorithmProvider(
        &hAlg, 
        BCRYPT_MD5_ALGORITHM, 
        NULL, 
        0);
    if (!BCRYPT_SUCCESS(status)) {
        log_error("Failed to open algorithm provider");
        result = ERROR_CRYPTO;
        goto cleanup;
    }

    // Get hash object size
    status = BCryptGetProperty(
        hAlg,
        BCRYPT_OBJECT_LENGTH,
        (PBYTE)&hash_length,
        sizeof(DWORD),
        &data_length,
        0);
    if (!BCRYPT_SUCCESS(status)) {
        log_error("Failed to get hash object size");
        result = ERROR_CRYPTO;
        goto cleanup;
    }

    // Allocate hash object
    hash_object = (PBYTE)HeapAlloc(GetProcessHeap(), 0, hash_length);
    if (!hash_object) {
        log_error("Failed to allocate hash object");
        result = ERROR_MEMORY_ALLOCATION;
        goto cleanup;
    }

    // Allocate hash data buffer (16 bytes for MD5)
    hash_data = (PBYTE)HeapAlloc(GetProcessHeap(), 0, 16);
    if (!hash_data) {
        log_error("Failed to allocate hash data buffer");
        result = ERROR_MEMORY_ALLOCATION;
        goto cleanup;
    }

    // Create hash
    status = BCryptCreateHash(
        hAlg,
        &hHash,
        hash_object,
        hash_length,
        NULL,
        0,
        0);
    if (!BCRYPT_SUCCESS(status)) {
        log_error("Failed to create hash");
        result = ERROR_CRYPTO;
        goto cleanup;
    }

    // Hash the data
    status = BCryptHashData(
        hHash,
        (PBYTE)patient_name,
        (ULONG)strlen(patient_name),
        0);
    if (!BCRYPT_SUCCESS(status)) {
        log_error("Failed to hash data");
        result = ERROR_CRYPTO;
        goto cleanup;
    }

    // Finalize hash
    status = BCryptFinishHash(
        hHash,
        hash_data,
        16,
        0);
    if (!BCRYPT_SUCCESS(status)) {
        log_error("Failed to finalize hash");
        result = ERROR_CRYPTO;
        goto cleanup;
    }

    // Convert to hex string
    for (int i = 0; i < 16; i++) {
        sprintf(&output[i * 2], "%02x", hash_data[i]);
    }
    output[32] = '\0';

cleanup:
    if (hHash) BCryptDestroyHash(hHash);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    if (hash_object) HeapFree(GetProcessHeap(), 0, hash_object);
    if (hash_data) HeapFree(GetProcessHeap(), 0, hash_data);

    return result;
}

/**
 * @brief Encodes current datetime in ISO format with custom separator
 */
ErrorCode encode_curr_datetime(char* output, size_t output_size) {
    if (!output || output_size < MAX_DATETIME_LENGTH) {
        return ERROR_INVALID_PARAM;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);

    // Format: YYYY-MM-DDTHH__MM__SS.mmmmmm (matching Python's isoformat with __ for :)
    int written = snprintf(output, output_size,
                         "%04d-%02d-%02dT%02d__%02d__%02d.%06d",
                         now.wYear,
                         now.wMonth,
                         now.wDay,
                         now.wHour,
                         now.wMinute,
                         now.wSecond,
                         now.wMilliseconds * 1000);  // Convert to microseconds

    if (written < 0 || written >= output_size) {
        log_error("DateTime string truncated");
        return ERROR_BUFF_OVERFLOW;
    }

    return ERROR_NONE;
}

/**
 * @brief Decodes an encoded datetime string back to SYSTEMTIME
 */
ErrorCode decode_encoded_datetime(const char* encoded_datetime, SYSTEMTIME* decoded_time) {
    if (!encoded_datetime || !decoded_time) {
        return ERROR_INVALID_PARAM;
    }

    int year, month, day, hour, min, sec, microsec;
    
    // Parse format: YYYY-MM-DDTHH__MM__SS.mmmmmm
    if (sscanf(encoded_datetime, "%d-%d-%dT%d__%d__%d.%d",
               &year, &month, &day, &hour, &min, &sec, &microsec) != 7) {
        log_error("Failed to parse datetime string: %s", encoded_datetime);
        return ERROR_TIME_CONVERSION;
    }

    memset(decoded_time, 0, sizeof(SYSTEMTIME));
    decoded_time->wYear = year;
    decoded_time->wMonth = month;
    decoded_time->wDay = day;
    decoded_time->wHour = hour;
    decoded_time->wMinute = min;
    decoded_time->wSecond = sec;
    decoded_time->wMilliseconds = microsec / 1000;  // Convert microseconds to milliseconds

    return ERROR_NONE;
}

/*MAY NEED THIS LATER unused for now*/
/*

static ErrorCode format_datetime_for_display(const SYSTEMTIME* time, char* buffer, size_t buffer_size) {
    if (!time || !buffer || buffer_size < 11) return ERROR_INVALID_PARAM;
    
    snprintf(buffer, buffer_size, "%02d-%02d-%04d",
             time->wMonth,
             time->wDay,
             time->wYear);
             
    return ERROR_NONE;
}
*/