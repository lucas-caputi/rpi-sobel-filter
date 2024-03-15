/******************************
 * File: to442_filters.cpp
 * 
 * Description: Source file containing image filtering functions.
 * 
 * Authors: Lucas Caputi
 * 
******************************/
#include "to442_filters.hpp"

/*-----------------------------
 * Function: to442_grayscale_and_sobel
 * 
 * Description: Applies grayscale filter and sobel filter to openCV matrix.
 * Assumes valid input arguments and that this function is being run by a thread.
 * 
 * Param args: void pointer to a structure of type to442_arg_data containing arguments.
 * 
 * return: NULL
-------------------------------*/
void *to442_grayscale_and_sobel(void *args) {
    while(!thread_exit_flag){
        pthread_barrier_wait(&thread_start_barrier);
        if (!thread_exit_flag) {    // necessary check to prevent thread from accessing empty image after video is finished
            to442_grayscale(
                *((struct to442_arg_data *)args)->in_mat, 
                *((struct to442_arg_data *)args)->grayscaled_mat,
                ((struct to442_arg_data *)args)->x,
                ((struct to442_arg_data *)args)->y,
                ((struct to442_arg_data *)args)->cols,
                ((struct to442_arg_data *)args)->rows
                );
            to442_sobel(
                *((struct to442_arg_data *)args)->grayscaled_mat, 
                *((struct to442_arg_data *)args)->out_mat,
                ((struct to442_arg_data *)args)->x,
                ((struct to442_arg_data *)args)->y,
                ((struct to442_arg_data *)args)->cols,
                ((struct to442_arg_data *)args)->rows
                );
        }
        pthread_barrier_wait(&thread_finished_barrier);
    }

    return NULL;
}

/*-----------------------------
 * Function: to442_grayscale
 * 
 * Description: Applies grayscale filter to openCV matrix.
 * 
 * Param in_mat: address of openCV matrix to apply filter to. 
 * Param out_mat: address of openCV matrix of filtered image.
 * Param x_tl: x-cordinate of top left pixel.
 * Param y_tl: y-cordinate of top left pixel.
 * Param cols: number of columns in matrix.
 * Param rows: number of rows in matrix.
 * 
 * return: an integer representing success or error.
 *         0 = success.
-------------------------------*/
int to442_grayscale(Mat &in_mat, Mat &out_mat, int x_tl, int y_tl, int cols, int rows) {
    int total_pixels = cols * rows;

    // filter using NEON intrinsics, 8 pixels at a time
    int pixels_filtered = 0;
    uint8_t *in_mat_ptr = in_mat.ptr<uint8_t>(y_tl, x_tl);
    uint8_t *out_mat_ptr = out_mat.ptr<uint8_t>(y_tl, x_tl);
    for(; pixels_filtered < total_pixels - 8; pixels_filtered += 8, in_mat_ptr += 24, out_mat_ptr += 8) {
        uint8x8x3_t rgb_vect = vld3_u8(in_mat_ptr);                 // read RGB values as uint8 of 8 pixels
        uint16x8_t r_vect = vmovl_u8(rgb_vect.val[RED]);            // "recast" R values from uint8 to uint16   
        uint16x8_t g_vect = vmovl_u8(rgb_vect.val[GREEN]);          // "recast" G values from uint8 to uint16   
        uint16x8_t b_vect = vmovl_u8(rgb_vect.val[BLUE]);           // "recast" B values from uint8 to uint16   
        r_vect = vmulq_n_u16(r_vect, 3);                            // vector multiply (0.2126 * R) (x16 ~= 3)
        g_vect = vmulq_n_u16(g_vect, 11);                           // vector multiply (0.7152 * G) (x16 ~= 11)
        b_vect = vmulq_n_u16(b_vect, 2);                            // vector multiply (0.0722 * B) (x16 ~= 2)
        r_vect = vshrq_n_u16(r_vect, 4);                            // vector shift to divide by 16
        g_vect = vshrq_n_u16(g_vect, 4);                            // vector shift to divide by 16
        b_vect = vshrq_n_u16(b_vect, 4);                            // vector shift to divide by 16
        uint16x8_t rg_vect = vaddq_u16(r_vect, g_vect);             // vector addition for R and G
        uint16x8_t gray_vect = vaddq_u16(rg_vect, b_vect);          // vector addition for RG and B
        uint8x8_t final_vect = vqmovn_u16(gray_vect);               // saturating narrow from uint16 to uint8
        vst1_u8(out_mat_ptr, final_vect);                           // set output frame's pixels to greyscaled pixels
    }

    // clean up (if pixel count is not divisible by 8)
    for(; pixels_filtered < total_pixels - 1; pixels_filtered++) {
        Vec3b pixel = in_mat.at<Vec3b>(pixels_filtered / cols, pixels_filtered % cols);
        uchar grayscaled_pixel = static_cast<uchar>(0.2126 * pixel[2] + 0.7152 * pixel[1] + 0.0722 * pixel[1]);
        out_mat.at<uchar>(pixels_filtered / cols, pixels_filtered % cols) = grayscaled_pixel;
    }
    
    return 0;
}

/*-----------------------------
 * Function: to442_sobel
 * 
 * Description: Applies Sobel filter to openCV matrix.
 * 
 * Param in_mat: address of openCV matrix to apply filter to. 
 * Param out_mat: address of openCV matrix of filtered image.
 * Param x_tl: x-cordinate of top left pixel
 * Param y_tl: y-cordinate of top left pixel
 * Param cols: number of columns in matrix
 * Param rows: number of rows in matrix
 * 
 * return: an integer representing success or error.
 *         0 = success.
-------------------------------*/
int to442_sobel(Mat &in_mat, Mat &out_mat, int x_tl, int y_tl, int cols, int rows) {
    const int16_t sobel_x_kernel[3][3] = {{-1, 0, 1},
                                        {-2, 0, 2},
                                        {-1, 0, 1}};
    const int16_t sobel_y_kernel[3][3] = {{-1, -2, -1},
                                        { 0,  0,  0},
                                        { 1,  2,  1}};
    uint8_t *in_mat_ptr = in_mat.ptr<uint8_t>(y_tl, x_tl);
    uint8_t *out_mat_ptr = out_mat.ptr<uint8_t>(y_tl, x_tl);

    // apply sobel filter 8 pixels at a time, a row at a time
    for(int y = 0; y < rows - 1; y++) {
        int x = 1;

        // calculate Gx for all 8 pixels
        for(; x < cols - 8; x += 8) {
            int16x8_t sobel_Gx = vdupq_n_s16(0);                                                        // initialize sobel_x vector
            for(int i = 0; i < 3; ++i) {
                for(int j = 0; j < 3; ++j) {
                    if(sobel_x_kernel[i][j] != 0) {
                        int16x8_t kernel_val = vdupq_n_s16(sobel_x_kernel[i][j]);                       // set all lanes to Gx value
                        uint8x8_t input_val = vld1_u8(in_mat_ptr + (y + i - 1) * cols + x + j - 1);     // load pixels
                        int16x8_t input_val_16 = vreinterpretq_s16_u16(vmovl_u8(input_val));            // widen pixel values to signed 16-bit
                        sobel_Gx = vmlaq_s16(sobel_Gx, input_val_16, kernel_val);                       // multiply and accumulate
                    }
                }
            }

            // calculate Gy for all 8 pixels
            int16x8_t sobel_Gy = vdupq_n_s16(0);                                                        // initialize sobel_y vector
            for(int i = 0; i < 3; ++i) {
                for(int j = 0; j < 3; ++j) {
                    if(sobel_y_kernel[i][j] != 0) {
                        int16x8_t kernel_val = vdupq_n_s16(sobel_y_kernel[i][j]);                       // set all lanes to Gy value
                        uint8x8_t input_val = vld1_u8(in_mat_ptr + (y + i - 1) * cols + x + j - 1);     // load pixels
                        int16x8_t input_val_16 = vreinterpretq_s16_u16(vmovl_u8(input_val));            // widen pixel values to signed 16-bit
                        sobel_Gy = vmlaq_s16(sobel_Gy, input_val_16, kernel_val);                       // multiply and accumulate
                    }
                }
            }

            // calculate G for all 8 pixels and apply it to output image
            int16x8_t sobel_vect = vaddq_s16(sobel_Gx, sobel_Gy);                                       // perform Gx + Gy for all 8 pixels
            uint8x8_t final_vect = vqmovun_s16(sobel_vect);                                             // convert back to 8-bit integers
            vst1_u8(out_mat_ptr + y * cols + x, final_vect);                                            // set output pixels
        }

        // handle the remaining columns if number of columns in image is not divisible by 8
        for(; x < cols - 1; x += 1) {
            int Gx = -in_mat.at<uchar>(y - 1, x - 1) - 2 * in_mat.at<uchar>(y, x - 1) - in_mat.at<uchar>(y + 1, x - 1) +
                    in_mat.at<uchar>(y - 1, x + 1) + 2 * in_mat.at<uchar>(y, x + 1) + in_mat.at<uchar>(y + 1, x + 1);
            int Gy = in_mat.at<uchar>(y - 1, x - 1) + 2 * in_mat.at<uchar>(y - 1, x) + in_mat.at<uchar>(y - 1, x + 1) -
                    in_mat.at<uchar>(y + 1, x - 1) - 2 * in_mat.at<uchar>(y + 1, x) - in_mat.at<uchar>(y + 1, x + 1);
            int G = Gx + Gy;
            out_mat.at<uchar>(y, x) = (G > 255) ? 255 : (G < 0) ? 0 : static_cast<uchar>(G);
        }
    }

    return 0;
}