/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 * 
 * Android JNI threads are created detached, meaning pthread_join() will not work.
 * These methods re-create the functionality of pthread_join on detached threads.
 * 
 */

#include "thread.h"
#include "gps.h"


Thread* thread_alloc( void )
{
    Thread *t = (Thread*) calloc( 1, sizeof( Thread ) );
    t->state = THREAD_STARTING;
    
    pthread_mutex_init(&t->mutex, NULL);
    pthread_cond_init(&t->signal, NULL);
    
    return t;
}

void thread_free( Thread *t )
{
    if ( !t ) return;
    
    pthread_cond_destroy( &t->signal );
    pthread_mutex_destroy( &t->mutex );
    
    free( t );
}

void thread_enter( Thread *t )
{
    pthread_mutex_lock(&t->mutex);
    t->state = THREAD_ENTERED;
    pthread_mutex_unlock(&t->mutex);
    
    pthread_cond_signal(&t->signal);
}


void thread_exit( Thread *t )
{
    pthread_mutex_lock(&t->mutex);
    t->state = THREAD_STOPPED;
    pthread_mutex_unlock(&t->mutex);
    
    pthread_cond_signal(&t->signal);
}

void thread_wait( Thread *t )
{
    int err;
    
    pthread_mutex_lock(&t->mutex);

    while ( t->state != THREAD_ENTERED ) {
        err = pthread_cond_wait(&t->signal, &t->mutex);
        if ( err ) {
            ERROR("failed at pthread_cond_wait: (%d)", err);
        }
    }
    
    pthread_mutex_unlock(&t->mutex);
}

void thread_join( Thread *t )
{
    int err;
    
    pthread_mutex_lock(&t->mutex);

    t->state = THREAD_EXITING;

    while ( t->state != THREAD_STOPPED ) {
        err = pthread_cond_wait(&t->signal, &t->mutex);
        if ( err ) {
            ERROR("failed at pthread_cond_wait: (%d)", err);
        }
    }
    
    pthread_mutex_unlock(&t->mutex);
}
