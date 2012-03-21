/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _MESSAGE_QUEUE_H_
#define _MESSAGE_QUEUE_H_

#include "message.h"
#include <pthread.h>

typedef struct _MessageQueueItem {
    struct _MessageQueueItem *next;
    Message *msg;
} MessageQueueItem;

typedef struct {
    pthread_cond_t signal;
    pthread_mutex_t mutex;
    
    MessageQueueItem *head;
    MessageQueueItem *tail;
} MessageQueue;

MessageQueue* message_queue_init( void );

void message_queue_free( MessageQueue *queue );

// message_queue_add
// adds the message to the queue and retains it
void message_queue_add( MessageQueue *queue, Message *msg );

// message_queue_remove
// removes the message from the queue and releases it
void message_queue_remove( MessageQueue *queue, Message *msg );

// message_queue_drain
// empties the queue and releases the messages
void message_queue_drain( MessageQueue *queue );

void message_queue_signal( MessageQueue *queue );

EXIT_CODE message_queue_wait_for_add( 
    MessageQueue *queue, 
    long timeout_ms
);

EXIT_CODE message_queue_wait_for_message( 
    MessageQueue *queue, 
    long timeout_ms, 
    MessageType msgType, 
    MessageId msgId,
    Message** msgPtr
);

EXIT_CODE message_queue_peek( 
    MessageQueue *queue, 
    int idx, 
    Message** msgPtr 
);

// message_queue_pop
// removes the first message from the queue but DOES NOT release it
// caller must release the returned message using message_release
EXIT_CODE message_queue_pop( 
    MessageQueue *queue,
    long timeout_ms, 
    Message** msgPtr 
);

#endif /* _MESSAGE_QUEUE_H_ */
