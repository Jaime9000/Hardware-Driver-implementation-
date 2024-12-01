#ifndef IMAGE_ROTATE_H
#define IMAGE_ROTATE_H

#include <opencv2/core/core_c.h>
#include "error_codes.h"

/**
 * @brief Rotates an image while keeping all content visible by adjusting bounds
 * 
 * @param image Input image to rotate
 * @param angle Rotation angle in degrees (positive = counterclockwise)
 * @param output Pointer to store the rotated image (will be allocated)
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 * 
 * Note: Caller is responsible for freeing the output image using cvReleaseImage
 */
ErrorCode rotate_bound(IplImage* image, double angle, IplImage** output);

#endif // IMAGE_ROTATE_H