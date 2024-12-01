#include "image_rotate.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <math.h>

ErrorCode rotate_bound(IplImage* image, double angle, IplImage** output) {
    if (!image || !output) {
        return ERROR_INVALID_PARAMETER;
    }

    *output = NULL;
    
    // Get image dimensions and center
    int height = image->height;
    int width = image->width;
    CvPoint2D32f center = cvPoint2D32f(width/2.0f, height/2.0f);

    // Create rotation matrix
    CvMat* rotation_matrix = cvCreateMat(2, 3, CV_32FC1);
    if (!rotation_matrix) {
        return ERROR_MEMORY_ALLOCATION;
    }

    // Get rotation matrix (negative angle for clockwise rotation)
    cv2DRotationMatrix(center, -angle, 1.0, rotation_matrix);

    // Get rotation components
    double cos_angle = fabs(CV_MAT_ELEM(*rotation_matrix, float, 0, 0));
    double sin_angle = fabs(CV_MAT_ELEM(*rotation_matrix, float, 0, 1));

    // Compute new bounding dimensions
    int new_width = (int)((height * sin_angle) + (width * cos_angle));
    int new_height = (int)((height * cos_angle) + (width * sin_angle));

    // Adjust rotation matrix translation components
    CV_MAT_ELEM(*rotation_matrix, float, 0, 2) += (new_width/2.0f) - center.x;
    CV_MAT_ELEM(*rotation_matrix, float, 1, 2) += (new_height/2.0f) - center.y;

    // Create output image
    *output = cvCreateImage(cvSize(new_width, new_height), image->depth, image->nChannels);
    if (!*output) {
        cvReleaseMat(&rotation_matrix);
        return ERROR_MEMORY_ALLOCATION;
    }

    // Perform the rotation
    ErrorCode error = ERROR_NONE;
    try {
        cvWarpAffine(image, *output, rotation_matrix,
                     CV_INTER_LINEAR + CV_WARP_FILL_OUTLIERS,
                     cvScalarAll(0));
    } catch (...) {
        cvReleaseImage(output);
        *output = NULL;
        error = ERROR_IMAGE_PROCESSING;
    }

    cvReleaseMat(&rotation_matrix);
    return error;
}