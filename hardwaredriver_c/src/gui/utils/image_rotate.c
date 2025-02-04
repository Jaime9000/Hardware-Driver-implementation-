#include "src/gui/utils/image_rotate.h"

// OpenCV headers
#include <opencv2/imgproc/imgproc_c.h>

// System headers
#include <math.h>

/**
 * @brief Rotates an image while keeping all content visible by adjusting bounds
 */
ErrorCode rotate_bound(IplImage* image, double angle, IplImage** output) {
    // Validate input parameters
    if (!image || !output) {
        return ERROR_INVALID_PARAMETER;
    }

    // Initialize output pointer
    *output = NULL;
    
    // Get image dimensions and center point
    int height = image->height;
    int width = image->width;
    CvPoint2D32f center = cvPoint2D32f(width/2.0f, height/2.0f);

    // Create rotation matrix
    CvMat* rotation_matrix = cvCreateMat(2, 3, CV_32FC1);
    if (!rotation_matrix) {
        return ERROR_MEMORY_ALLOCATION;
    }

    // Calculate rotation matrix (negative angle for clockwise rotation)
    cv2DRotationMatrix(center, -angle, 1.0, rotation_matrix);

    // Extract rotation components
    double cos_angle = fabs(CV_MAT_ELEM(*rotation_matrix, float, 0, 0));
    double sin_angle = fabs(CV_MAT_ELEM(*rotation_matrix, float, 0, 1));

    // Calculate new dimensions to contain rotated image
    int new_width = (int)((height * sin_angle) + (width * cos_angle));
    int new_height = (int)((height * cos_angle) + (width * sin_angle));

    // Adjust matrix translation to center the image
    CV_MAT_ELEM(*rotation_matrix, float, 0, 2) += (new_width/2.0f) - center.x;
    CV_MAT_ELEM(*rotation_matrix, float, 1, 2) += (new_height/2.0f) - center.y;

    // Create output image with new dimensions
    *output = cvCreateImage(cvSize(new_width, new_height), 
                          image->depth, 
                          image->nChannels);
    if (!*output) {
        cvReleaseMat(&rotation_matrix);
        return ERROR_MEMORY_ALLOCATION;
    }

    // Perform the rotation
    int result = cvWarpAffine(image, 
                             *output, 
                             rotation_matrix,
                             CV_INTER_LINEAR + CV_WARP_FILL_OUTLIERS,
                             cvScalarAll(0));  // Fill background with black

    // Check for errors
    if (result != 0) {  // OpenCV returns 0 on success
        cvReleaseImage(output);
        *output = NULL;
        cvReleaseMat(&rotation_matrix);
        return ERROR_IMAGE_PROCESSING;
    }

    // Cleanup
    cvReleaseMat(&rotation_matrix);
    return ERROR_NONE;
}