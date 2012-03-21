/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _READ_THREAD_H_
#define _READ_THREAD_H_

#include "gps_state.h"
#include "message_queue.h"

typedef struct {
    volatile int            active;
    int                     control[2];
    Thread*                 thread;
    GpsState*               gpsState;
    MessageQueue*           msgQueue;
} ReadThread;

ReadThread* read_thread_init( GpsState* s );
void read_thread_free( ReadThread* t );

EXIT_CODE read_thread_start( ReadThread* t );
EXIT_CODE read_thread_stop( ReadThread* t );

#endif /* _READ_THREAD_H_ */
