#include "src/gui/utils/play_video.h"
#include "src/gui/utils/video_defaults.h"
#include "src/core/logger.h"

// System headers
#include <stdio.h>
#include <string.h>
#include <windows.h>

// Third-party headers
#include "cJSON.h"

// Constants
#define MAX_JSON_BUFFER_SIZE 8192
#define MAX_ID_STRING_LENGTH 16

/**
 * @brief Loads existing JSON file or creates new one with defaults
 * 
 * @param[out] json Pointer to store the loaded/created JSON object
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 */
static ErrorCode load_or_create_json(cJSON** json) {
    if (!json) return ERROR_INVALID_PARAM;

    FILE* fp = fopen(JSON_PATH, "r");
    if (!fp) {
        // Create new JSON file with defaults
        *json = cJSON_CreateObject();
        if (!*json) {
            log_error("Failed to create JSON object");
            return ERROR_MEMORY_ALLOCATION;
        }

        // Add default video mappings
        for (size_t i = 0; i < VIDEO_MAPPING_COUNT; i++) {
            const VideoMapping* mapping = &DEFAULT_VIDEO_MAPPINGS[i];
            char id_str[MAX_ID_STRING_LENGTH];
            snprintf(id_str, sizeof(id_str), "%u", mapping->id);
            
            if (mapping->name) {
                if (!cJSON_AddStringToObject(*json, id_str, mapping->name)) {
                    log_error("Failed to add mapping for ID %s", id_str);
                    cJSON_Delete(*json);
                    return ERROR_MEMORY_ALLOCATION;
                }
            } else {
                if (!cJSON_AddNullToObject(*json, id_str)) {
                    log_error("Failed to add null mapping for ID %s", id_str);
                    cJSON_Delete(*json);
                    return ERROR_MEMORY_ALLOCATION;
                }
            }
        }

        // Save to file
        char* json_str = cJSON_Print(*json);
        if (!json_str) {
            log_error("Failed to serialize JSON");
            cJSON_Delete(*json);
            return ERROR_MEMORY_ALLOCATION;
        }

        fp = fopen(JSON_PATH, "w");
        if (!fp) {
            log_error("Failed to create JSON file");
            free(json_str);
            cJSON_Delete(*json);
            return ERROR_FILE_OPERATION;
        }

        fputs(json_str, fp);
        fclose(fp);
        free(json_str);
        return ERROR_NONE;
    }

    // Read existing JSON
    char buffer[MAX_JSON_BUFFER_SIZE];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    
    buffer[bytes_read] = '\0';
    *json = cJSON_Parse(buffer);
    if (!*json) {
        log_error("Failed to parse JSON file");
        return ERROR_FILE_OPERATION;
    }

    return ERROR_NONE;
}

ErrorCode play_video_file(uint32_t video_id) {
    // Load or create JSON mapping
    cJSON* json = NULL;
    ErrorCode error = load_or_create_json(&json);
    if (error != ERROR_NONE) return error;

    // Look up video name
    char id_str[MAX_ID_STRING_LENGTH];
    snprintf(id_str, sizeof(id_str), "%u", video_id);
    
    cJSON* video_name_json = cJSON_GetObjectItem(json, id_str);
    if (!video_name_json || !video_name_json->valuestring) {
        log_error("Invalid video ID: %u", video_id);
        cJSON_Delete(json);
        return ERROR_INVALID_VIDEO;
    }

    // Build video path
    char video_path[MAX_PATH];
    snprintf(video_path, sizeof(video_path), VIDEO_PATH, video_name_json->valuestring);

    // Check if file exists
    if (GetFileAttributesA(video_path) == INVALID_FILE_ATTRIBUTES) {
        log_error("Cannot find video file: %s", video_path);
        cJSON_Delete(json);
        return ERROR_FILE_NOT_FOUND;
    }

    // Open video using ShellExecute
    HINSTANCE result = ShellExecuteA(NULL, "open", video_path, NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)result <= 32) {  // Windows API defines success as > 32
        log_error("Failed to open video file: %s", video_path);
        cJSON_Delete(json);
        return ERROR_FILE_OPERATION;
    }

    cJSON_Delete(json);
    return ERROR_NONE;
}