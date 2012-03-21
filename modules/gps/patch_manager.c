/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * OSP tracker functions
 *
 */

#include "patch_manager.h"
#include "patch_manager_types.h"

#include "gps_datetime.h"
#include "patch_file.h"
#include "osp_msg.h"
#include "work_thread_sirf.h"

EXIT_CODE _pm_wait_for_ack( WorkState* s, OspMinorId minorId, UINT16 seqNumber )
{
    long timeout;
    TIME expire;
    
    Message* ackMsg;
    PatchManagerAck ack;
    
    timeout = PM_TIMEOUT_MS;
    expire = NOW() + timeout;

    while (s->active) {
    
        FAIL_UNLESS( message_queue_wait_for_message( 
            s->readQueue,
            timeout,
            MSG_TYPE_OSP,
            OSP_ID_PM_ACK,
            &ackMsg
        ));
        
        ack = *((PatchManagerAck*)ackMsg->obj);
        message_queue_remove( s->readQueue, ackMsg );
        
        if ( ack.minorId != minorId ) {
            WARN("non-matching id: 0x%02x != 0x%02x", ack.minorId, minorId);
        } else if ( ack.seqNumber != seqNumber ) {
            WARN("non-matching sequence: 0x%04x != 0x%04x", ack.seqNumber, seqNumber);
        } else {
            if ( ack.status != PATCH_MANAGER_ACK ) {
                WARN("NACK: %d", ack.status);
                return EXIT_FAILURE;
            }
            
            // DEBUG("ACK");
            return EXIT_SUCCESS;
        }
        
        timeout = expire - NOW();
        if ( timeout < 0 ) break;
    }
    
    return EXIT_FAILURE;
}

EXIT_CODE _pm_send_start_request( WorkState* s, PatchManagerPrompt* prompt )
{
    Message *msg;
    Message *promptMsg;
    
    INFO("");
        
    // send Patch Manager Start Request
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_PM_START_REQ;
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );
    
    // wait for Patch Manager Prompt
    FAIL_UNLESS( message_queue_wait_for_message( 
        s->readQueue,
        PM_TIMEOUT_MS,
        MSG_TYPE_OSP,
        OSP_ID_PM_PROMPT,
        &promptMsg
    ));
    
    *prompt = *( (PatchManagerPrompt*)(promptMsg->obj) );
    
    message_queue_remove( s->readQueue, promptMsg );
    return EXIT_SUCCESS;
}

EXIT_CODE _pm_send_load_request( WorkState* s, UINT16 seqNum, PatchManagerLoadRequest* request )
{
    Message *msg;
    
    INFO("#%d length=%d", seqNum, request->dataLen);
    
    // send Patch Memory Load Request
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_PM_LOAD_REQ;
    msg->obj = COPY( *request );
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );
    
    // wait for Patch Manager Acknowledgement
    return _pm_wait_for_ack( s, OSP_ID_MINOR( OSP_ID_PM_LOAD_REQ ), seqNum );
}

EXIT_CODE _pm_send_exit_request( WorkState* s )
{
    Message *msg;
    
    INFO("");
    
    // send Patch Memory Exit Request
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_PM_EXIT_REQ;
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );
    
    // wait for Patch Manager Acknowledgement
    return _pm_wait_for_ack( s, OSP_ID_MINOR( OSP_ID_PM_EXIT_REQ ), 0 );
}

EXIT_CODE pm_send_patch_file( WorkState* s, const char* path )
{
    int result = EXIT_FAILURE;

    PatchFile* patchFile;
    PatchManagerPrompt prompt;
    PatchManagerLoadRequest request;
    
    UINT32 length;
    UINT8 finalPkt = 0;

    patchFile = patch_file_init( path );
    if ( patchFile == NULL ) {
        ERROR("failed to load patch file: %s", path);
        goto end;
    }
    
    // send Patch Manager Start Request
    // and wait for Patch Manager Prompt
    if (FAILED( _pm_send_start_request( s, &prompt ) )) {
        ERROR("failed to send Patch Manager Start Request");
        goto end;
    }
    
    // check that chip, rom and version numbers match up
    if (FAILED( patch_file_validate( patchFile, &prompt ) )) {
        ERROR("failed to validate patch against chip");
        goto end;
    }
    
    // send file data
    while (s->active) {
    
        memset( &request, 0, sizeof( request ));
        
        if (FAILED( patch_file_get_next_request( patchFile, &request, &finalPkt ) )) {
            ERROR("failed to send Patch Manager Load Request");
            break;
        }
        
        // send Patch Memory Load Request
        // and wait for Patch Manager Acknowledgement
        if (FAILED( _pm_send_load_request( s, patchFile->state.seqNum, &request ) )) {
            ERROR("failed to send Patch Manager Load Request");
            break;
        }
        
        // done
        if ( finalPkt ) {
            result = EXIT_SUCCESS;
            break;
        }
    }

    // send Patch Manager Exit Request
    // wait for Patch Manager Acknowledgement
    if (FAILED( _pm_send_exit_request( s ) )) {
        /* NOTE: Since we are not using EEPROM for storage, we will always 
         * get a NACK response.  Ignore and check if patch was successful
         * via software version string or PM prompt.
         */
    }
        
    if (FAILED( sirf_wait_for_ok_to_send( s ) )) {
        ERROR("did not receive OkToSend");
    }
        
    // validate that patch took
    if ( result == EXIT_SUCCESS ) {

        if (FAILED( _pm_send_start_request( s, &prompt ) )) {
            ERROR("failed to send Patch Manager Start Request again");
            goto end;
        }
        _pm_send_exit_request( s );
        
        if ( patchFile->header.patchRevisionCode != prompt.patchVersion ) {
            ERROR("patch did not take! file=0x%04x chip=0x%04x", 
                patchFile->header.patchRevisionCode, prompt.patchVersion );
            result = EXIT_FAILURE;
        }
    }
    
end:
    if ( patchFile ) patch_file_free( patchFile );
    return result;
}


