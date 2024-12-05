#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <bcrypt.h>
#include "logger.h"

#pragma comment(lib, "bcrypt.lib")

ErrorCode encode_name(const char* patient_name, char* output, size_t output_size) {
    if (!patient_name || !output || output_size < MAX_HASH_LENGTH) {
        return ERROR_INVALID_PARAMETER;
    }

    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    DWORD hash_length = 0;
    DWORD data_length = 0;
    PBYTE hash_object = NULL;
    PBYTE hash_data = NULL;
    ErrorCode result = ERROR_NONE;

    // Open algorithm provider
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
        result = ERROR_MEMORY;
        goto cleanup;
    }

    // Allocate hash data buffer (16 bytes for MD5)
    hash_data = (PBYTE)HeapAlloc(GetProcessHeap(), 0, 16);
    if (!hash_data) {
        log_error("Failed to allocate hash data buffer");
        result = ERROR_MEMORY;
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

ErrorCode encode_curr_datetime(char* output, size_t output_size) {
    if (!output || output_size < MAX_DATETIME_LENGTH) {
        return ERROR_INVALID_PARAMETER;
    }

    time_t now;
    struct tm tm_now;
    
    time(&now);
    if (localtime_s(&tm_now, &now) != 0) {
        log_error("Failed to get local time");
        return ERROR_TIME_CONVERSION;
    }

    // Format: YYYY-MM-DDTHH__MM__SS.mmmmmm
    int written = snprintf(output, output_size,
                         "%04d-%02d-%02dT%02d__%02d__%02d.000000",
                         tm_now.tm_year + 1900,
                         tm_now.tm_mon + 1,
                         tm_now.tm_mday,
                         tm_now.tm_hour,
                         tm_now.tm_min,
                         tm_now.tm_sec);

    if (written < 0 || written >= output_size) {
        log_error("DateTime string truncated");
        return ERROR_BUFFER_OVERFLOW;
    }

    return ERROR_NONE;
}

ErrorCode decode_encoded_datetime(const char* encoded_datetime, struct tm* decoded_time) {
    if (!encoded_datetime || !decoded_time) {
        return ERROR_INVALID_PARAMETER;
    }

    int year, month, day, hour, min, sec;
    
    // Parse format: YYYY-MM-DDTHH__MM__SS.mmmmmm
    if (sscanf(encoded_datetime, "%d-%d-%dT%d__%d__%d",
               &year, &month, &day, &hour, &min, &sec) != 6) {
        log_error("Failed to parse datetime string");
        return ERROR_TIME_CONVERSION;
    }

    memset(decoded_time, 0, sizeof(struct tm));
    decoded_time->tm_year = year - 1900;
    decoded_time->tm_mon = month - 1;
    decoded_time->tm_mday = day;
    decoded_time->tm_hour = hour;
    decoded_time->tm_min = min;
    decoded_time->tm_sec = sec;

    return ERROR_NONE;
}