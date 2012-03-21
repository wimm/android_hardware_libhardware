/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _WRITE_THREAD_H_
#define _WRITE_THREAD_H_

#include "gps_state.h"
#include "message_queue.h"

typedef struct {
    volatile int            active;
    Thread*                 thread;
    GpsState*               gpsState;
    MessageQueue*           msgQueue;
} WriteThread;

WriteThread* write_thread_init( GpsState* s );
void write_thread_free( WriteThread* t );

EXIT_CODE write_thread_start( WriteThread* t );
EXIT_CODE write_thread_stop( WriteThread* t );

#endif /* _WRITE_THREAD_H_ */
