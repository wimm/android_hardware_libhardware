/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */

#include "work_thread.h"

#include "work_thread_eclm.h"
#include "work_thread_geo.h"
#include "work_thread_sirf.h"

#include "nmea_types.h"
#include "osp_types.h"

#include "power.h"
#include "storage.h"

void osp_handle_msg( WorkThread* t, Message *msg )
{
    switch ( OSP_ID_MAJOR( msg->id ) )
    {
        case OSP_MID_MEASURED_NAVIGATION_DATA:
            DEBUG("OSP_MID_MEASURED_NAVIGATION_DATA");
            break;
        
        case OSP_MID_MEASURED_TRACKER_DATA:
            DEBUG("OSP_MID_MEASURED_TRACKER_DATA");
            break;
        
        case OSP_MID_CLOCK_STATUS_DATA:
            DEBUG("OSP_MID_CLOCK_STATUS_DATA");
            break;
        
        case OSP_MID_CPU_THROUGHPUT:
            DEBUG("OSP_MID_CPU_THROUGHPUT");
            break;
            
        case OSP_MID_OK_TO_SEND: {
            OspOkToSend* obj = (OspOkToSend*)msg->obj;
            INFO("OkToSend: %s", ( obj->okToSend ? "YES" : "NO" ));
        } break;
        
        case OSP_MID_COMMAND_ACK: {
            OspCommandAck* obj = (OspCommandAck*)msg->obj;
            INFO("ACK: 0x%04x", obj->msgId);
        } break;
        
        case OSP_MID_COMMAND_NACK: {
            OspCommandNack* obj = (OspCommandNack*)msg->obj;
            INFO("NACK: 0x%04x", obj->msgId);
        } break;
        
        case OSP_MID_GEODETIC_NAVAGATION:
            DEBUG("OSP_MID_GEODETIC_NAVAGATION");
            break;
        
        case OSP_MID_EE_OUTPUT: {
            eclm_handle_msg( t, msg );
        } break;
        
        case OSP_MID_CW_OUTPUT:
            DEBUG("OSP_MID_CW_OUTPUT");
            break;
            
        case OSP_MID_TCXO_LEARNING_OUT:
            DEBUG("OSP_MID_TCXO_LEARNING_OUT");
            break;
        
        default: {
            WARN("unhandled OSP message: id=0x%04x", msg->id);
            // PRINT_DATA( msg->obj, msg->objLen );
        } break;
    }
}

void _work_thread_handle_msg( WorkThread* t, Message *msg )
{
    // signal sgee thread
    // it may be waiting for comm change
    WorkState *sgeeWorkState = &t->sgeeThread->workState;
    message_queue_signal( sgeeWorkState->readQueue );

    // DEBUG("0x%02x/0x%04x", msg->type, msg->id);
    
    switch ( msg->type )
    {
        case MSG_TYPE_GEO:
            work_thread_geo_handle_msg( t, msg );
            break;
            
        case MSG_TYPE_OSP:
            osp_handle_msg( t, msg );
            break;

        // all incoming MSG_TYPE_SIRF should have been
        // converted into MSG_TYPE_OSP
            
        default:
            WARN("unhandled message: type=0x%x id=0x%x", msg->type, msg->id);
            break;
    }
}

EXIT_CODE _work_thread_hibernate( WorkThread* t )
{
    int timeout = GPS_WAIT_FOR_IDLE_DELAY;
    TIME now, start, lastPulse = 0;
    
    INFO("");
    
    pulse_gps_power();
    
    // The orderly shutdown may take anywhere from 10ms to 900ms 
    // to complete depending on operation.
    while ( timeout > 0 ) {
    
        start = NOW();
        
        // wait up to 900ms for messages to stop arriving
        if (FAILED( message_queue_wait_for_add( t->workState.readQueue, timeout ) )) {
            DEBUG("entered hibernate");
            return EXIT_SUCCESS;
        }
        
        now = NOW();
        timeout -= ( now - start );
    }
    
    ERROR("failed to enter hibernate");
    return EXIT_FAILURE;
}

void _work_thread_powerup( WorkThread* t )
{
    int attempt = 0;
    GpsState* s = t->workState.gpsState;
    
    int timeout = GPS_WAIT_FOR_POWER_DELAY;
    TIME now, delta, lastPulse = 0;
    
    INFO("");
    
    while ( 1 ) {
    
        // for dealing with open issue where chip is reset when device suspend/resumes
        if ( attempt == 3 ) {
            ERROR("reverting to 4800 baud");
            gps_comm_cfg( &s->comm, GPS_BAUD_4800 );
            gps_state_set_flag( s, GPS_FLAG_HAS_PATCH, 0 );
        }
        
        // wait for a message to arrive
        if (OK( message_queue_wait_for_add( t->workState.readQueue, timeout ) )) {
            DEBUG("message received");
            return;
        }
        
        if ( !t->workState.active ) 
            return;
        
        now = NOW();
        delta = now - lastPulse;
        
        if ( delta >= 1000 ) {
            pulse_gps_power();
            lastPulse = now;
            timeout = 1000;
            attempt++;
        } else {
            timeout = 1000 - delta;
        }
    }
}

static void work_thread( void* arg )
{
    WorkThread* t = (WorkThread*)arg;
    GpsState* s = t->workState.gpsState;
    
    // init seems to cause chip to re-request SGEE on warm start
    // disabling until we can figure out what's wrong
    int needsInit = 0;
    Message* msg;
    
    INFO("starting");
    thread_enter( t->thread );
    
    gps_state_set_status( s, GPS_STATUS_SESSION_BEGIN );
    gps_state_dispatch( s, GPS_DISPATCH_STATUS );
    
    // pulse power til we get a message
    _work_thread_powerup( t );    
    if ( !t->workState.active ) goto end;

    // switch to high-speed NMEA
    if (FAILED( sirf_set_comm( &t->workState, GPS_PROTO_NMEA, GPS_BAUD_57600 ) ))
        ERROR("failed to set protocol to NMEA @ %d", GPS_BAUD_57600);
    
    // patch ROM
    if ( gps_state_get_flag( s, GPS_FLAG_HAS_PATCH ) == 0 ) {
        if (FAILED( sirf_patch_rom( &t->workState ) )) goto end;
        if ( !t->workState.active ) goto end;
    }
    
    // TODO
    // sirf_poll_sgee_age( &t->workState );
    
    // send SGEE if processed file does not exist
    if (FAILED( storage_exists( GPS_NVM_STORAGE_SGEE ) )) {
        sgee_thread_start( t->sgeeThread );
    }
    
    // clear messages that arrived during setup
    message_queue_drain( t->workState.readQueue );
    
    INFO("started");
    
    // wait for incoming messages
    while ( t->workState.active ) {

        // initial setup
        // wait for SGEE to finish before init
        if ( needsInit && !t->sgeeThread->workState.active ) {
            sirf_init_nav( &t->workState, SIRF_RESET_WARM_WITH_INIT );
            needsInit = 0;
        }
        
        // read messages
        if (OK( message_queue_pop( t->workState.readQueue, -1, &msg ) )) {
            _work_thread_handle_msg( t, msg );
            message_release( msg );
        }
    }
    
end:
    INFO("exiting (active=%d)", t->workState.active);
    
    _work_thread_hibernate( t );

    gps_state_set_status( s, GPS_STATUS_SESSION_END );
    gps_state_dispatch( s, GPS_DISPATCH_STATUS );
    
    // cleanup
    if ( t->workState.active ) {
        t->workState.active = 0;   
        thread_free( t->thread );
        t->thread = NULL;
        
        gps_state_release_wakelock( s );
    } else {    
        thread_exit( t->thread );
    }
    
    INFO("exited");
}

EXIT_CODE work_thread_start( WorkThread* t )
{
    if ( t->thread ) {
        WARN("thread already running");
        return EXIT_SUCCESS;
    }
    
    INFO("");
    gps_state_acquire_wakelock( t->workState.gpsState );
    
    t->workState.active = 1;
    t->thread = gps_state_create_thread( 
        t->workState.gpsState, "gps_work_thread", work_thread, t
    );
    
    if ( !t->thread ) {
        t->workState.active = 0;
        gps_state_release_wakelock( t->workState.gpsState );
        return EXIT_FAILURE;
    }
    
    // wait for thread to start
    thread_wait( t->thread );
    
    return EXIT_SUCCESS;
}

EXIT_CODE work_thread_stop( WorkThread* t )
{
    if ( !t->thread ) {
        WARN("thread not running");
        return EXIT_SUCCESS;
    }
    
    // stop sgee thread
    if ( t->sgeeThread->workState.active ) {
        sgee_thread_stop( t->sgeeThread );
    }
    
    // wait for thread to quit
    INFO("waiting for thread to exit");
    t->workState.active = 0;
    message_queue_signal( t->workState.readQueue );
    thread_join( t->thread );    
    thread_free( t->thread );
    t->thread = NULL;
    INFO("thread exited");
    
    gps_state_release_wakelock( t->workState.gpsState );
    
    return EXIT_SUCCESS;
}

WorkThread* work_thread_init( WorkState* s ) {
    WorkState sgeeState;
    WorkThread* t = (WorkThread*) calloc( 1, sizeof( WorkThread ) );
    t->workState = *s;
    
    sgeeState.active = 0;
    sgeeState.gpsState = s->gpsState;
    sgeeState.readQueue = message_queue_init();
    sgeeState.writeQueue = s->writeQueue;
    t->sgeeThread = sgee_thread_init( &sgeeState );
    
    return t;
}

void work_thread_free( WorkThread* t )
{
    if ( !t ) return;
    if ( t->thread ) ERROR("thread freed before exited!");
    
    if ( t->sgeeThread ) {
        MessageQueue *rq = t->sgeeThread->workState.readQueue;
        sgee_thread_free( t->sgeeThread );
        message_queue_free( rq );
    }
    
    free( t );
}
