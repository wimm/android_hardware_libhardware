/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _WORK_THREAD_H_
#define _WORK_THREAD_H_

#include "message.h"
#include "thread.h"
#include "work_state.h"
#include "sgee_thread.h"

typedef struct {
    Thread*                 thread;
    WorkState               workState;
    SgeeThread*             sgeeThread;
} WorkThread;


WorkThread* work_thread_init( WorkState* s );
void work_thread_free( WorkThread* t );

EXIT_CODE work_thread_start( WorkThread* t );
EXIT_CODE work_thread_stop( WorkThread* t );

EXIT_CODE work_thread_send_msg( WorkThread* t, Message* msg );

#endif /* _WORK_THREAD_H_ */
