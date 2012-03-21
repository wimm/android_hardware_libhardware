/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>

typedef enum {
    THREAD_STARTING,
    THREAD_ENTERED,
    THREAD_EXITING,
    THREAD_STOPPED
} ThreadState;

typedef struct {
    ThreadState state;
    pthread_t thread;
    
    pthread_cond_t signal;
    pthread_mutex_t mutex;
} Thread;

Thread* thread_alloc( void );
void thread_free( Thread *t );

// call from a thread when it has started
void thread_enter( Thread *t );

// call from a thread when it is about to exit
void thread_exit( Thread *t );

// block until a thread has entered
void thread_wait( Thread *t );

// block until a thread has exited
void thread_join( Thread *t );

#endif /* _THREAD_H_ */
