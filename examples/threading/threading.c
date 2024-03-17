#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define MILLI_TO_MICRO (1000)

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    struct thread_data* thread_func_args = (struct thread_data*)thread_param;

    //Sleep
    usleep(thread_func_args->wait_to_obtain_ms * MILLI_TO_MICRO);

    //obtain mutex
    pthread_mutex_lock(thread_func_args->mutex);

    //Sleep
    usleep(thread_func_args->wait_to_release_ms * MILLI_TO_MICRO);

    //Release mutex
    pthread_mutex_unlock(thread_func_args->mutex);

    //Thread completion status
    thread_func_args->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    //Allocate memory for threads_data
    struct thread_data* thread_data_ptr = (struct thread_data*)malloc(sizeof(struct thread_data));
    if(thread_data_ptr == NULL)
    {
        ERROR_LOG("Memory allocation failed");
	return false;
    }

    //thread_data parameters
    thread_data_ptr->mutex=mutex;
    thread_data_ptr->wait_to_obtain_ms=wait_to_obtain_ms;
    thread_data_ptr->wait_to_release_ms=wait_to_release_ms;
    thread_data_ptr->thread_complete_success = false;

    //create a thread
    int create_thread_result = pthread_create(thread, NULL, threadfunc, (void*)thread_data_ptr);

    // Check if thread creation was successful
    if (create_thread_result != 0) 
    {
        free(thread_data_ptr);
	ERROR_LOG("Thread creation failed");
        return false;  // Thread creation failed
    }
    return true;
}

