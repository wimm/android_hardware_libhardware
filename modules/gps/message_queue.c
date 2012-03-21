/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * NMEA message queue functions
 *
 */


#include "message_queue.h"

#include "gps_datetime.h"

void _message_queue_add( MessageQueue *queue, Message *msg )
{
    MessageQueueItem *item = (MessageQueueItem*) calloc( 1, sizeof( MessageQueueItem ) );
    item->msg = message_retain( msg );

    DEBUG_MSG_QUEUE("%p [0x%02x/0x%04x]", queue, msg->type, msg->id);

    // queue is empty
    if ( !queue->head ) {
        queue->head = item;
        if ( queue->tail ) {
            ERROR("orphaned tail! potential leak detected.");
        }
        queue->tail = item;
    } 
    
    // add to queue
    else {
        MessageQueueItem *lastItem = queue->tail;
        lastItem->next = item;
        queue->tail = item;
    }
}

void _message_queue_remove( MessageQueue *queue, Message *msg )
{
    MessageQueueItem *item = queue->head;
    MessageQueueItem *prevItem = NULL;

    while ( item != NULL ) {
        if ( item->msg == msg ) break;
        prevItem = item;
        item = item->next;
    }
    
    if ( item != NULL ) {
        DEBUG_MSG_QUEUE("%p [0x%02x/0x%04x]", queue, msg->type, msg->id);
    
        if ( prevItem ) {
            prevItem->next = item->next;
        } else {
            queue->head = item->next;
        }

        if ( queue->tail == item ) 
            queue->tail = prevItem;
            
        free( item );
    }
}

EXIT_CODE _message_queue_pop( MessageQueue *queue, Message** msgPtr )
{
    Message *msg;
    MessageQueueItem *item, *nextItem;
    
    if ( !queue->head ) return EXIT_FAILURE;
    
    // get the message and next item
    item = queue->head;
    nextItem = item->next;
    msg = item->msg;

    DEBUG_MSG_QUEUE("%p [0x%02x/0x%04x]", queue, msg->type, msg->id);
    
    // pop the item off the queue
    queue->head = nextItem;
    if ( !queue->head ) queue->tail = NULL;
    free(item);
    
    if ( msgPtr ) *msgPtr = msg;
    return EXIT_SUCCESS;
}

EXIT_CODE _message_queue_wait( MessageQueue *queue, long timeout_ms )
{
    int err;
    
    // no wait
    if ( timeout_ms == 0 ) 
        return EXIT_FAILURE;
    
    // untimed wait
    if ( timeout_ms < 0 ) {
        err = pthread_cond_wait(&queue->signal, &queue->mutex);
        if ( err ) {
            ERROR("%p failed at pthread_cond_wait: (%d) %s", queue, err, strerror(err));
            return EXIT_FAILURE;
        }

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

        DEBUG_MSG_QUEUE("%p waiting %ld ms", queue, timeout_ms);

        err = pthread_cond_timedwait(&queue->signal, &queue->mutex, &expire);
        if ( err ) {
            ERROR("%p failed at pthread_cond_timedwait: (%d) %s", queue, err, strerror(err));
            return EXIT_FAILURE;
        }
    }
    
    return EXIT_SUCCESS;
}

void message_queue_signal( MessageQueue *queue ) 
{
    pthread_cond_signal(&queue->signal);
}

EXIT_CODE message_queue_wait_for_add( 
    MessageQueue *queue, 
    long timeout_ms
) {
    EXIT_CODE res = EXIT_FAILURE;
    pthread_mutex_lock(&queue->mutex);
    res = _message_queue_wait( queue, timeout_ms );
    pthread_mutex_unlock(&queue->mutex);
    return res;
}

EXIT_CODE message_queue_wait_for_message( 
    MessageQueue *queue, 
    long timeout_ms, 
    MessageType msgType, 
    MessageId msgId,
    Message** msgPtr
) {
    int idx;
    long timeout;
    TIME expire;
    
    Message *msg;

    timeout = timeout_ms;
    expire = NOW() + timeout;

    DEBUG_MSG_QUEUE("%p 0x%02x/0x%04x", queue, msgType, msgId);
    
    // wait for a new message to arrive
    while (OK( message_queue_wait_for_add( queue, timeout ) )) {
        idx = 0;
    
        while (OK( message_queue_peek( queue, idx++, &msg ) )) {
            DEBUG_MSG_QUEUE("idx[%d] 0x%02x/0x%04x", idx, msg->type, msg->id);
            if ( msg->type != msgType ) continue;            
            if ( msg->id != msgId ) continue;
            if ( msgPtr ) *msgPtr = msg;
            return EXIT_SUCCESS;
        }
        
        timeout = expire - NOW();
        if ( timeout < 0 ) break;
    }
    
    return EXIT_FAILURE;
}

EXIT_CODE message_queue_peek( 
    MessageQueue *queue, 
    int idx,
    Message** msgPtr
) {
    int i = 0;
    MessageQueueItem *item;
    
    pthread_mutex_lock(&queue->mutex);
    
    item = queue->head;
    while ( item != NULL ) {
        if ( idx == i ) break;
        item = item->next;
        i++;
    }
    
    pthread_mutex_unlock(&queue->mutex);

    if ( item && msgPtr ) *msgPtr = item->msg;
    return ( item ? EXIT_SUCCESS : EXIT_FAILURE );
}

EXIT_CODE message_queue_pop( 
    MessageQueue *queue, 
    long timeout_ms, 
    Message** msgPtr
) {
    Message *msg = NULL;
    EXIT_CODE res = EXIT_FAILURE;
    
    pthread_mutex_lock(&queue->mutex);

    // data already exists
    if ( queue->head ) 
        goto pop;

    // wait for data to arrive
    if (FAILED( _message_queue_wait( queue, timeout_ms ) )) 
        goto done;

pop:
    res = _message_queue_pop( queue, msgPtr );
    
done:
    pthread_mutex_unlock(&queue->mutex);

    return res;
}

void message_queue_add( MessageQueue *queue, Message *msg )
{
    pthread_mutex_lock(&queue->mutex);
    _message_queue_add( queue, msg );
    pthread_mutex_unlock(&queue->mutex);
    pthread_cond_signal(&queue->signal);
}

void message_queue_remove( MessageQueue *queue, Message *msg )
{
    pthread_mutex_lock(&queue->mutex);
    _message_queue_remove( queue, msg );
    pthread_mutex_unlock(&queue->mutex);
}

void message_queue_drain( MessageQueue *queue )
{
    MessageQueueItem *item;

    pthread_mutex_lock(&queue->mutex);
    
    // free queued messages    
    while (( item = queue->head ) != NULL) {
        queue->head = item->next;
        message_release( item->msg );
        free( item );
    }
    
    queue->tail = NULL;
    
    pthread_mutex_unlock(&queue->mutex);
}

MessageQueue* message_queue_init( void )
{
    pthread_mutexattr_t attr;
    MessageQueue *q = (MessageQueue*) calloc( 1, sizeof( MessageQueue ) );
    
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&q->mutex, &attr);
    pthread_cond_init(&q->signal, NULL);
    
    return q;
}

void message_queue_free( MessageQueue *queue )
{
    message_queue_drain( queue );

    pthread_cond_destroy( &queue->signal );
    pthread_mutex_destroy( &queue->mutex );
    
    free( queue );
}

