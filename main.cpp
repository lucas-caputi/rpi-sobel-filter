/******************************
 * File: main.cpp
 * 
 * Description: Main source file for sobel filter program.
 * 
 * Authors: Lucas Caputi
 * 
******************************/
#include "to442_filters.hpp"

// global flag declaration
pthread_barrier_t thread_start_barrier;
pthread_barrier_t thread_finished_barrier;
int thread_exit_flag;

/*-----------------------------
 * Function: main
 * 
 * Description: Applies sobel filter to video given by a command line argument.
 * 
 * return: an integer representing success or error.
 *         -2 = usage error
 *         -1 = error opening video
 *          0 = success 
-------------------------------*/
int main(int argc, char **argv) {
    // global barrier definition
    pthread_barrier_init(&thread_start_barrier, NULL, 5);
    pthread_barrier_init(&thread_finished_barrier, NULL, 5);

    // global thread flag definition
    thread_exit_flag = false;

    // local variables
    Mat in_frame;
    pthread_t tid[NUM_OF_THREADS];

    // check for correct number of arguments
    if(argc != 2) {
        cout << "Usage error" << endl;
        return -2;
    }

    // create window to view video
    namedWindow("Video", WINDOW_AUTOSIZE);

    // get video from command line arg
    VideoCapture video(argv[1]);
    if(video.isOpened() == false) {
        cout << "\n Error: cannot open video. Check file path. \n";
        return -1;
    }

    // get frame width and height
    int frame_cols = video.get(CAP_PROP_FRAME_WIDTH);
    int frame_rows = video.get(CAP_PROP_FRAME_HEIGHT);

    // create a new mat to hold greyscaled frame image
    Mat grayscaled_frame = Mat(frame_rows, frame_cols, CV_8U);

    // create a new mat to hold filtered frame image (with 8 bit unsigned value per pixel)
    Mat out_frame = Mat(frame_rows, frame_cols, CV_8U);

    // create structs to hold frame data for threads (ul and br get an extra row/column to avoid intermediate borders)
    struct to442_arg_data frame_data_one;    // upper left portion of frame
    frame_data_one.x = 0;
    frame_data_one.y = 0;
    frame_data_one.cols = frame_cols;
    frame_data_one.rows = (frame_rows / 4) + 1;
    frame_data_one.in_mat = &in_frame;
    frame_data_one.grayscaled_mat = &grayscaled_frame;
    frame_data_one.out_mat = &out_frame;
    struct to442_arg_data frame_data_two;    // upper right portion of frame
    frame_data_two.x = 0;
    frame_data_two.y = frame_rows / 4;
    frame_data_two.cols = frame_cols;
    frame_data_two.rows = (frame_rows / 4) + 1;
    frame_data_two.in_mat = &in_frame;
    frame_data_two.grayscaled_mat = &grayscaled_frame;
    frame_data_two.out_mat = &out_frame;
    struct to442_arg_data frame_data_three;    // bottom left portion of frame
    frame_data_three.x = 0;
    frame_data_three.y =  2 * (frame_rows / 4);
    frame_data_three.cols = frame_cols;
    frame_data_three.rows = (frame_rows / 4) + 1;
    frame_data_three.in_mat = &in_frame;
    frame_data_three.grayscaled_mat = &grayscaled_frame;
    frame_data_three.out_mat = &out_frame;
    struct to442_arg_data frame_data_four;    // bottom right portion of frame
    frame_data_four.x = 0;
    frame_data_four.y = 3 * (frame_rows / 4);
    frame_data_four.cols = frame_cols;
    frame_data_four.rows = frame_rows / 4;
    frame_data_four.in_mat = &in_frame;
    frame_data_four.grayscaled_mat = &grayscaled_frame;
    frame_data_four.out_mat = &out_frame;

    // make 4 threads to each filter a portion of the image
    pthread_create(&tid[0], NULL, &to442_grayscale_and_sobel, &frame_data_one);
    pthread_create(&tid[1], NULL, &to442_grayscale_and_sobel, &frame_data_two);
    pthread_create(&tid[2], NULL, &to442_grayscale_and_sobel, &frame_data_three);
    pthread_create(&tid[3], NULL, &to442_grayscale_and_sobel, &frame_data_four);

    // intialize timer and frame count for FPS
    int frame_count = 0;
    clock_t start, end;
    start = clock();

    // perform filtering on video
    while(true) {
        // read a frame, checking if video is finished
        if(!video.read(in_frame)) {
            thread_exit_flag = true;
            pthread_barrier_wait(&thread_start_barrier);
            pthread_barrier_wait(&thread_finished_barrier);
            for(int i = 0; i < NUM_OF_THREADS; i++) {
                pthread_join(tid[i], NULL);
            }
            pthread_barrier_destroy(&thread_start_barrier);
            pthread_barrier_destroy(&thread_finished_barrier);
            break;
        }

        // begin thread execution
        pthread_barrier_wait(&thread_start_barrier);

        // increment frame count
        frame_count++;

        // wait for all four threads finish filtering frame
        pthread_barrier_wait(&thread_finished_barrier);

        // display frame
        imshow("Video", out_frame);

        // break the loop if the 'ESC' key is pressed
        if (waitKey(30) == 27) {
            break;
        }
    }

    // stop timer and calculate fps
    end = clock();
    printf("Average fps = %f\n", (frame_count / (((double) (end - start)) / CLOCKS_PER_SEC)));

    // cleanup
    video.release();
    destroyAllWindows();

    return 0;
}
