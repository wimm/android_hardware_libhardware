/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include "gps.h"
#include <pthread.h>


#if DBG_MSG_IO
#define DEBUG_MSG_IO(...)               DEBUG(__VA_ARGS__)
#else
#define DEBUG_MSG_IO(...)               ((void)0)
#endif /* DBG_MSG_IO */

#if DBG_MSG_QUEUE
#define DEBUG_MSG_QUEUE(...)            DEBUG(__VA_ARGS__)
#else
#define DEBUG_MSG_QUEUE(...)            ((void)0)
#endif /* DBG_MSG_QUEUE */



typedef UINT8 MessageType;
enum _MessageType {
    MSG_TYPE_GEO                = 0x00,
    MSG_TYPE_SIRF               = 0x10,
    MSG_TYPE_OSP                = 0x20,
};

typedef UINT32 MessageId;

typedef void (*message_obj_dispose)(void* msg);

typedef struct {
    MessageType type;
    MessageId id;
    
    message_obj_dispose dispose;
    void* obj;
    
    void* internal;
} Message;

// message_init 
// Allocates and returns a new Message object with a retain count of 1.
Message* message_init( void );

// message_free
// Free the message.  You should not call this directly.  Use message_release instead.
// If the "dispose" callback is set, the callback is invoked with the message.
// If "obj" is set, it is freed.
// Then "msg" is freed.
void message_free( Message *msg );

// message_retain
// Increments retain count by 1.
Message* message_retain( Message *msg );

// message_release
// Decrements retain count by 1.  
// The message is freed when retain count == 0.
void message_release( Message *msg );

// message_wait_until_sent
EXIT_CODE message_wait_until_sent( Message *msg, long timeout_ms );

// message_signal_sent
void message_signal_sent( Message *msg );
            
#endif /* _MESSAGE_H_ */
