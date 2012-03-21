/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */

#include "write_thread.h"

#include "buffer.h"

#include "nmea_msg.h"
#include "osp_msg.h"

EXIT_CODE _write_thread_send_msg( WriteThread* t, Message* msg )
{
    EXIT_CODE res = EXIT_FAILURE;
    
    Buffer b;
    int maxSize;
    GpsProtocol protocol = gps_state_get_protocol( t->gpsState );
    
    memset( &b, 0, sizeof(b) );

    // DEBUG("0x%02x/0x%04x", msg->type, msg->id);
            
    switch ( protocol )
    {
        case GPS_PROTO_OSP: {
            b.len = OSP_MAX_SIZE;
            b.data = malloc( b.len );
            if (FAILED( osp_msg_write( msg, &b ) )) goto end;
        } break;
            
        case GPS_PROTO_NMEA: {
            // add 1 for null terminator
            b.len = NMEA_MAX_SIZE+1;
            b.data = malloc( b.len );
            if (FAILED( nmea_msg_write( msg, &b ) )) goto end;
        } break;
            
        default:
            ERROR("unknown protocol: 0x%x", protocol);
            return EXIT_FAILURE;
    }
    
    res = gps_comm_write( &t->gpsState->comm, b.data, b.pos );

end:
    free( b.data );
    return res;
}

static void write_thread( void* arg )
{
    WriteThread* t = (WriteThread*)arg;
    Message *msg = NULL;
    
    INFO("starting");
    thread_enter( t->thread );
    
    // wait for messages
    while ( t->active ) {
        if (OK( message_queue_pop( t->msgQueue, -1, &msg ) )) {
            _write_thread_send_msg( t, msg );
            message_signal_sent( msg );
            message_release( msg );
        }
    }
    
exit:
    INFO("exiting (active=%d)", t->active);
    
    if ( t->active ) {
        t->active = 0;
        thread_free( t->thread );
        t->thread = NULL;
    } else {
        thread_exit( t->thread );
    }
    
    INFO("exited");
}


/*
 * THREAD CONTROL
 */

EXIT_CODE write_thread_start( WriteThread* t ) 
{
    if ( t->thread ) {
        WARN("thread already running");
        return EXIT_SUCCESS;
    }
    
    INFO("");
    
    t->active = 1;
    t->thread = gps_state_create_thread( t->gpsState, "gps_write_thread", write_thread, t );
    if ( !t->thread ) {
        t->active = 0;
        return EXIT_FAILURE;
    }
    
    thread_wait( t->thread );
    return EXIT_SUCCESS;
}

EXIT_CODE write_thread_stop( WriteThread* t )
{
    if ( !t->thread ) {
        WARN("thread not running");
        return EXIT_SUCCESS;
    }
    
    // wait for thread to quit
    INFO("waiting for thread to exit");
    t->active = 0;
    message_queue_signal( t->msgQueue );
    thread_join( t->thread );
    thread_free( t->thread );
    t->thread = NULL;
    INFO("thread exited");
    
    return EXIT_SUCCESS;
}

WriteThread* write_thread_init( GpsState* s ) {
    WriteThread* t = (WriteThread*) calloc( 1, sizeof( WriteThread ) );
    
    t->gpsState = s;
    t->msgQueue = message_queue_init();
    
    return t;
}

void write_thread_free( WriteThread* t )
{
    if ( !t ) return;
    
    if ( t->thread )
        ERROR("thread freed before self exited!");
    
    message_queue_free( t->msgQueue );
    
    free( t );
}
