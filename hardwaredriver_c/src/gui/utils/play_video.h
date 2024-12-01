#ifndef PLAY_VIDEO_H
#define PLAY_VIDEO_H

#include <stdint.h>
#include "error_codes.h"

#define JSON_PATH "c:\\k7\\video_options.json"
#define VIDEO_PATH "C:\\K7\\videos\\%s.m4v"

/**
 * @brief Plays a video file based on the video ID
 * 
 * @param video_id The ID of the video to play
 * @return ErrorCode ERROR_NONE on success, otherwise:
 *         ERROR_FILE_NOT_FOUND if JSON or video file missing
 *         ERROR_INVALID_VIDEO if video ID not found in mapping
 *         ERROR_FILE_OPEN if file operations fail
 */
ErrorCode play_video_file(uint32_t video_id);

#endif // PLAY_VIDEO_H