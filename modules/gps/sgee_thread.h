/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _SGEE_THREAD_H_
#define _SGEE_THREAD_H_

#include "work_state.h"

#define GPS_SGEE_PROTO              GPS_PROTO_OSP
#define GPS_SGEE_RATE               GPS_BAUD_57600
#define GPS_SGEE_MAX_BUFFER_SIZE    400

typedef struct {
    Thread*                 thread;
    WorkState               workState;
} SgeeThread;

SgeeThread* sgee_thread_init( WorkState* s );
void sgee_thread_free( SgeeThread* t );

EXIT_CODE sgee_thread_start( SgeeThread* t );
EXIT_CODE sgee_thread_stop( SgeeThread* t );

#endif /* _SGEE_THREAD_H_ */
