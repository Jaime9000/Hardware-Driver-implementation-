#ifndef IMAGE_ROTATE_H
#define IMAGE_ROTATE_H

// OpenCV headers
#include <opencv2/core/core_c.h>

// Project headers
#include "src/core/error_codes.h"

/**
 * @brief Rotates an image while keeping all content visible by adjusting bounds
 * 
 * This function performs a rotation of the input image around its center while
 * automatically adjusting the output image size to ensure no content is clipped.
 * The rotation is performed counterclockwise for positive angles.
 * 
 * @param image Input image to rotate (must not be NULL)
 * @param angle Rotation angle in degrees (positive = counterclockwise)
 * @param output Pointer to store the rotated image (will be allocated)
 * @return ErrorCode ERROR_NONE on success, error code otherwise
 * 
 * NOTE @note Caller is responsible for freeing the output image using cvReleaseImage
 * @note The output image dimensions will be adjusted to fit the rotated content
 * @note Background pixels will be filled with black (0)
 */
ErrorCode rotate_bound(IplImage* image, double angle, IplImage** output);

#endif // IMAGE_ROTATE_H