#ifndef PLAY_VIDEO_H
#define PLAY_VIDEO_H

// System headers

#include <stdint.h>

// Project headers
#include "src/core/error_codes.h"

// Path constants
#define JSON_PATH "c:\\k7\\video_options.json"
#define VIDEO_PATH "C:\\K7\\videos\\%s.m4v"

/**
 * @brief Plays a video file based on the video ID
 * 
 * This function looks up the video name from a JSON mapping file and plays
 * the corresponding video using the system's default media player.
 * If the JSON file doesn't exist, it will be created with default mappings.
 * 
 * @param video_id The ID of the video to play
 * @return ErrorCode ERROR_NONE on success, otherwise:
 *         ERROR_FILE_NOT_FOUND if JSON or video file missing
 *         ERROR_INVALID_VIDEO if video ID not found in mapping
 *         ERROR_FILE_OPERATION if file operations fail
 *         ERROR_MEMORY_ALLOCATION if memory allocation fails
 *         ERROR_FILE_OPERATION if JSON parsing fails
 */
ErrorCode play_video_file(uint32_t video_id);

#endif // PLAY_VIDEO_H