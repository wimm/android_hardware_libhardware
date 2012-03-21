/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * NMEA message queue functions
 *
 */


#include "message.h"


typedef struct {
    volatile int retainCount;
    volatile int sentFlag;
    pthread_cond_t signal;
    pthread_mutex_t mutex;
} MessageInternal;


EXIT_CODE message_wait_until_sent( Message *msg, long timeout_ms )
{
    EXIT_CODE res = EXIT_FAILURE;
    int err;
    MessageInternal *intl = (MessageInternal*)msg->internal;
    
    pthread_mutex_lock(&intl->mutex);
    
    if ( intl->sentFlag ) {
        res = EXIT_SUCCESS;
        goto end;
    }
    
    // no wait
    if ( timeout_ms == 0 ) goto end;
    
    // untimed wait
    if ( timeout_ms < 0 ) {
        err = pthread_cond_wait(&intl->signal, &intl->mutex);
        if ( err ) {
            ERROR("%p [0x%02x/0x%04x] failed at pthread_cond_wait: (%d) %s", 
                msg, msg->type, msg->id, err, strerror(err));
            goto end;
        }
        if ( intl->sentFlag ) res = EXIT_SUCCESS;

    // timed wait
    } else {
        struct timeval now;
        struct timespec expire;
        
        // wait for data
        gettimeofday(&now, NULL);
        now.tv_usec += timeout_ms * 1000;
        now.tv_sec += now.tv_usec / 1000000;
        now.tv_usec %= 1000000;
        
        expire.tv_sec = now.tv_sec;
        expire.tv_nsec = now.tv_usec * 1000;

        DEBUG_MSG_QUEUE("%p [0x%02x/0x%04x] waiting %ld ms", 
            msg, msg->type, msg->id, timeout_ms);

        err = pthread_cond_timedwait(&intl->signal, &intl->mutex, &expire);
        if ( err ) {
            ERROR("%p [0x%02x/0x%04x] failed at pthread_cond_timedwait: (%d) %s", 
                msg, msg->type, msg->id, err, strerror(err));
                
            goto end;
        }
        if ( intl->sentFlag ) res = EXIT_SUCCESS;
    }
end:
DEBUG_MSG_QUEUE("exit");

    // reset sent flag so others can resend the message
    intl->sentFlag = 0;
    
    pthread_mutex_unlock(&intl->mutex);
    return res;
}

void message_signal_sent( Message *msg )
{
    MessageInternal *intl = (MessageInternal*)msg->internal;

DEBUG_MSG_QUEUE("enter");
    pthread_mutex_lock(&intl->mutex);
    
DEBUG_MSG_QUEUE("signal");
    intl->sentFlag = 1;
    pthread_cond_signal(&intl->signal);
    
DEBUG_MSG_QUEUE("exit");
    pthread_mutex_unlock(&intl->mutex);
}

Message* message_retain( Message *msg )
{
    MessageInternal *intl = (MessageInternal*)msg->internal;
    intl->retainCount++;

    DEBUG_MSG_QUEUE("%p [0x%02x/0x%04x] retainCount=%d", 
        msg, msg->type, msg->id, intl->retainCount);
        
    return msg;
}

void message_release( Message *msg )
{
    MessageInternal *intl = (MessageInternal*)msg->internal;
    intl->retainCount--;
    
    DEBUG_MSG_QUEUE("%p [0x%02x/0x%04x] retainCount=%d", 
        msg, msg->type, msg->id, intl->retainCount);
        
    if ( intl->retainCount == 0 ) 
        message_free( msg );
}

Message* message_init( void ) 
{
    MessageInternal *intl = calloc( 1, sizeof( MessageInternal ) );
    pthread_mutex_init(&intl->mutex, NULL);
    pthread_cond_init(&intl->signal, NULL);
    intl->retainCount = 1;
    
    Message *msg = calloc( 1, sizeof( Message ) );
    msg->internal = intl;
    return msg;
}

void message_free( Message *msg )
{
    MessageInternal *intl;

    if ( !msg ) return;
    
    if ( msg->dispose ) {
        DEBUG_MSG_QUEUE("%p [0x%02x/0x%04x] dispose obj", msg, msg->type, msg->id);
        msg->dispose( msg );
    }
    
    if ( msg->obj ) {
        DEBUG_MSG_QUEUE("%p [0x%02x/0x%04x] free obj", msg, msg->type, msg->id);
        free( msg->obj );
    }

    DEBUG_MSG_QUEUE("%p [0x%02x/0x%04x] free intl", msg, msg->type, msg->id);
    intl = (MessageInternal*)msg->internal;
    pthread_cond_destroy( &intl->signal );
    pthread_mutex_destroy( &intl->mutex );
    free( intl );

    DEBUG_MSG_QUEUE("%p [0x%02x/0x%04x] free msg", msg, msg->type, msg->id);
    free( msg );
    DEBUG_MSG_QUEUE("done");
}

