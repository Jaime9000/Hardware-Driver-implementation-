#include "play_video.h"
#include "video_defaults.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "cJSON.h"
#include "logger.h"

static ErrorCode load_or_create_json(cJSON** json) {
    FILE* fp = fopen(JSON_PATH, "r");
    if (!fp) {
        // Create new JSON file with defaults
        *json = cJSON_CreateObject();
        if (!*json) return ERROR_MEMORY_ALLOCATION;

        for (size_t i = 0; i < VIDEO_MAPPING_COUNT; i++) {
            const VideoMapping* mapping = &DEFAULT_VIDEO_MAPPINGS[i];
            char id_str[16];
            snprintf(id_str, sizeof(id_str), "%u", mapping->id);
            if (mapping->name) {
                cJSON_AddStringToObject(*json, id_str, mapping->name);
            } else {
                cJSON_AddNullToObject(*json, id_str);
            }
        }

        // Save to file
        char* json_str = cJSON_Print(*json);
        if (!json_str) {
            cJSON_Delete(*json);
            return ERROR_MEMORY_ALLOCATION;
        }

        fp = fopen(JSON_PATH, "w");
        if (!fp) {
            free(json_str);
            cJSON_Delete(*json);
            return ERROR_FILE_OPEN;
        }

        fputs(json_str, fp);
        fclose(fp);
        free(json_str);
        return ERROR_NONE;
    }

    // Read existing JSON
    char buffer[8192];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    
    buffer[bytes_read] = '\0';
    *json = cJSON_Parse(buffer);
    if (!*json) return ERROR_FILE_READ;

    return ERROR_NONE;
}

ErrorCode play_video_file(uint32_t video_id) {
    cJSON* json = NULL;
    ErrorCode error = load_or_create_json(&json);
    if (error != ERROR_NONE) return error;

    // Look up video name
    char id_str[16];
    snprintf(id_str, sizeof(id_str), "%u", video_id);
    
    cJSON* video_name_json = cJSON_GetObjectItem(json, id_str);
    if (!video_name_json || !video_name_json->valuestring) {
        cJSON_Delete(json);
        return ERROR_INVALID_VIDEO;
    }

    // Build video path
    char video_path[MAX_PATH];
    snprintf(video_path, sizeof(video_path), VIDEO_PATH, video_name_json->valuestring);

    // Check if file exists
    if (GetFileAttributesA(video_path) == INVALID_FILE_ATTRIBUTES) {
        log_error("Cannot find video file @ %s", video_path);
        cJSON_Delete(json);
        return ERROR_FILE_NOT_FOUND;
    }

    // Open video using ShellExecute
    HINSTANCE result = ShellExecuteA(NULL, "open", video_path, NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)result <= 32) {
        log_error("Failed to open video file: %s", video_path);
        cJSON_Delete(json);
        return ERROR_FILE_OPEN;
    }

    cJSON_Delete(json);
    return ERROR_NONE;
}