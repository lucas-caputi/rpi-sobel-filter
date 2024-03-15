/******************************
 * File: to442_filters.hpp
 * 
 * Description: Header file for image filtering functions.
 * 
 * Authors: Lucas Caputi
 * 
******************************/
#ifndef _TO442_FILTERS_H
#define _TO442_FILTERS_H

/* includes */
#include <opencv2/opencv.hpp> 
#include <pthread.h>
#include <arm_neon.h>
#include <time.h>

/* defines */
#define NUM_OF_THREADS 4
#define RED 2
#define GREEN 1
#define BLUE 0

/* namespace definitions */
using namespace cv;
using namespace std;

// global variable decleration
extern pthread_barrier_t thread_start_barrier;
extern pthread_barrier_t thread_finished_barrier;
extern int thread_exit_flag;

/* function prototypes */
void *to442_grayscale_and_sobel(void *args);
int to442_grayscale(Mat &in_mat, Mat &out_mat, int x, int y, int cols, int rows);
int to442_sobel(Mat &in_mat, Mat &out_mat, int x, int y, int cols, int rows);

/* structure definitions */
struct to442_arg_data {
    int x;  // upper left x-cord
    int y;  // upper left y-cord
    int cols;
    int rows;
    Mat *in_mat;
    Mat *grayscaled_mat;
    Mat *out_mat;
};

#endif /* _TO442_FILTERS_H */