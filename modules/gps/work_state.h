/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _WORK_STATE_H_
#define _WORK_STATE_H_

#include "gps_state.h"
#include "message_queue.h"

typedef struct {
    volatile int            active;
    GpsState*               gpsState;
    MessageQueue*           readQueue;
    MessageQueue*           writeQueue;
} WorkState;

#endif /* _WORK_STATE_H_ */
