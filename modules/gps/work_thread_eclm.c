/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * ECLM message handler functions
 *
 */

#include "work_thread_eclm.h"

#include "sgee_thread.h"

#include "nmea_types.h"
#include "osp_types.h"

#include "buffer.h"
#include "storage.h"


#if DBG_EE
#define DEBUG_EE(...)                   INFO(__VA_ARGS__)
#else
#define DEBUG_EE(...)                   ((void)0)
#endif /* DBG_EE */



/*
 * STORAGE
 */

#define ECLM_STORAGE_HEADER     GPS_STORAGE_PATH "/headers"
            
const char* eclm_storage_path( EclmStorageId nvmId )
{
    switch ( nvmId ) {
        case ECLM_STORAGE_SGEE:
            return GPS_NVM_STORAGE_SGEE;
            
        case ECLM_STORAGE_CGEE:
            return GPS_NVM_STORAGE_CGEE;
            
        case ECLM_STORAGE_BE:
            return GPS_NVM_STORAGE_BE;
    
        default:
            ERROR("unknown NVM id: %d", nvmId);
    }
    
    return NULL;
}

EclmResultIn eclm_storage_erase( 
    EclmStorageId id 
) {
    const char *path = eclm_storage_path( id );

    if ( path == NULL ) 
        return ECLM_RES_I_INVALID_NVM;
    
    if (FAILED( storage_erase( path ) ))
        return ECLM_RES_I_FILE_ACCESS_ERR;
        
    return ECLM_RES_I_SUCCESS;
};

EclmResultIn eclm_storage_write( 
    EclmStorageId id,
    size_t size,
    off_t offset,
    void* data
) {
    const char *path = eclm_storage_path( id );

    if ( path == NULL ) 
        return ECLM_RES_I_INVALID_NVM;
    
    if ( mkdir( GPS_STORAGE_PATH, S_IRWXU ) == -1 ) {
        if ( errno != EEXIST ) {
            ERROR("error at mkdir: path=%s errno=%d %s", GPS_STORAGE_PATH, errno, strerror(errno));
            return ECLM_RES_I_FILE_ACCESS_ERR;
        }
    }
    
    if (FAILED( storage_write( path, size, offset, data ) ))
        return ECLM_RES_I_FILE_ACCESS_ERR;
        
    return ECLM_RES_I_SUCCESS;
}

EclmResultIn eclm_storage_read( 
    EclmStorageId id,
    size_t size,
    off_t offset,
    void* data,
    int* readLen
) {
    int len;
    const char *path = eclm_storage_path( id );

    if ( path == NULL ) 
        return ECLM_RES_I_INVALID_NVM;
    
    len = storage_read( path, size, offset, data );
    if ( len < 0 )
        return ECLM_RES_I_FILE_ACCESS_ERR;
    
    if ( readLen ) *readLen = len;
    return ECLM_RES_I_SUCCESS;
}


/*
 * MESSAGES
 */
 
EXIT_CODE eclm_wait_for_ack( 
    WorkState* s, 
    EclmMessageId msgId, 
    EclmResultOut* result
) {
    Message *msg;

    int idx = 0;
    long timeout;
    TIME expire;
    
    EclmAckNackResponse ackNackResponse;
    EclmAckNack ack;
    
    timeout = ECLM_ACK_TIMEOUT_MS;
    expire = NOW() + timeout;
    
    while (s->active) {
    
        FAIL_UNLESS( message_queue_wait_for_message( 
            s->readQueue,
            timeout,
            MSG_TYPE_OSP,
            OSP_ID_ECLM_ACK_NACK,
            &msg
        ));
    
        ackNackResponse = *((EclmAckNackResponse*)msg->obj);   
        message_queue_remove( s->readQueue, msg );
        
        if ( ackNackResponse.eclmId != msgId ) {
            WARN("id mismatch 0x%x != 0x%x", ackNackResponse.eclmId, msgId);
        } else {
            ack = ackNackResponse.ackNack;
            if ( result ) *result = ackNackResponse.result;
            
            if ( ack != ECLM_ACK ) {
                WARN("NACK 0x%x: %d", msgId, ackNackResponse.result);
                return EXIT_FAILURE;
            }
            
//             DEBUG("ACK 0x%x", msgId);
            return EXIT_SUCCESS;
        }
        
        timeout = expire - NOW();
        if ( timeout < 0 ) break;
    }
    
    if ( result ) *result = ECLM_RES_O_TIMEOUT;
    return EXIT_FAILURE;
}

EXIT_CODE eclm_send_sgee_start_download(
    WorkState* s,
    EclmResultOut* result
) {
    Message *msg;
    
    DEBUG_EE("");
    
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_ECLM_START_DOWNLOAD;
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );

    return eclm_wait_for_ack(s, OSP_ID_MINOR( OSP_ID_ECLM_START_DOWNLOAD ), result );
}

EXIT_CODE eclm_send_sgee_file_size(
    WorkState* s,
    UINT32 fileLength,
    EclmResultOut* result
) {
    Message *msg;
    EclmSgeeDownloadFileSize obj;
    
    DEBUG_EE("fileLength=%d", fileLength);
    
    memset( &obj, 0, sizeof(obj) );
    obj.fileLength = fileLength;
    
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_SGEE_DL_FILE_SIZE;
    msg->obj = COPY( obj );
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );

    return eclm_wait_for_ack(s, OSP_ID_MINOR( OSP_ID_SGEE_DL_FILE_SIZE ), result );
}

EXIT_CODE eclm_send_sgee_file_content(
    WorkState* s,
    EclmSgeeDownloadFileContent* obj,
    EclmResultOut* result
) {
    Message *msg;
    
    DEBUG_EE("#%d length=%d", obj->seqNum, obj->dataLen);
    
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_SGEE_DL_PKT_DATA;
    msg->obj = COPY( *obj );
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );
        
    return eclm_wait_for_ack(s, OSP_ID_MINOR( OSP_ID_SGEE_DL_PKT_DATA ), result );
}

// TODO  ECLM_I_EE_AGE_REQ

EXIT_CODE eclm_send_sgee_age_request(
    WorkState* s,
    UINT8 satPrn,
    EclmResultOut* result
) {
    Message *msg;
    EclmSgeeAgeRequest obj;
    
    DEBUG_EE("satPrn=%d", satPrn);
    
    memset( &obj, 0, sizeof(obj) );
    obj.satPrn = satPrn;
    
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_SGEE_AGE_REQ;
    msg->obj = COPY( obj );
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );
    
    return eclm_wait_for_ack(s, OSP_ID_MINOR( OSP_ID_SGEE_AGE_REQ ), result );
}

EXIT_CODE eclm_send_host_file_content(
    WorkState* s,
    EclmHostFileContent* obj
) {
    Message *msg;
    
    DEBUG_EE("#%d nvmId=%d", obj->seqNum, obj->nvmId);
    
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_HOST_FILE_CONTENT;
    msg->obj = COPY( *obj );
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );
    
    return EXIT_SUCCESS;
}

EXIT_CODE eclm_send_ack_nack(
    WorkState* s,
    EclmMessageId eclmId, 
    EclmResultIn result
) {
    Message *msg;
    EclmAckNackResponse obj;
    
    DEBUG_EE("eclmId=0x%x result=%d", eclmId, result);
    
    memset( &obj, 0, sizeof(obj) );
    obj.eclmId = eclmId;
    obj.ackNack = ( result ? ECLM_NACK : ECLM_ACK );
    obj.result = result;
    
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_HOST_ACK_NACK;
    msg->obj = COPY( obj );
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );
    
    return EXIT_SUCCESS;
}

EXIT_CODE eclm_send_ee_header_response(
    WorkState* s,
    OspEeHeaderResp* obj,
    EclmResultOut* result
) {
    Message *msg;
    
    DEBUG_EE("#%d dataLen=%d dataOffset=%d", obj->seqNum, obj->dataLen, obj->dataOffset);
    
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_EE_HEADER_RESP;
    msg->obj = COPY( obj );
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );
    
    return eclm_wait_for_ack(s, OSP_ID_MINOR( OSP_ID_EE_HEADER_RESP ), result );
}


/*
 * MESSAGE HANDLERS
 */

void eclm_handle_ack_nack( WorkThread* t, EclmAckNackResponse* data ) 
{
    WARN("uncaught ack/nack 0x%x", data->eclmId);
}

void eclm_handle_ee_age_response( WorkThread* t, EclmEeAgeResponse* data ) 
{
    WARN("uncaught");
}

void eclm_handle_sgee_age_response( WorkThread* t, EclmSgeeAgeResponse* data ) 
{
    WARN("uncaught");
}

void eclm_handle_sgee_request( WorkThread* t, EclmSgeeRequest* data ) 
{
    DEBUG_EE("start=%d delay=%d", 
        data->start, 
        data->delay
    );
 
    if ( data->start == 0 ) {     
        if ( !t->sgeeThread->workState.active ) {
            WARN("SGEE thread not running");
        } else {
            sgee_thread_stop( t->sgeeThread );
            eclm_send_ack_nack(&t->workState, ECLM_O_SGEE_REQ, ECLM_RES_I_SUCCESS );
        }
    } else if ( data->delay == 0 ) {            
        if ( t->sgeeThread->workState.active ) {
            WARN("SGEE thread already running");
        } else {
            eclm_send_ack_nack(&t->workState, ECLM_O_SGEE_REQ, ECLM_RES_I_SUCCESS );
            sgee_thread_start( t->sgeeThread );
        }
    } else {
        ERROR("TODO: delayed download");
        eclm_send_ack_nack(&t->workState, ECLM_O_SGEE_REQ, ECLM_RES_I_FILE_ACCESS_ERR );
    }
}

void eclm_handle_file_erase( WorkThread* t, EclmFileErase* data ) 
{
    EclmResultIn result;
    
    DEBUG_EE("nvmId=%d", data->nvmId);
    result = eclm_storage_erase(data->nvmId);
    eclm_send_ack_nack(&t->workState, ECLM_O_FILE_ERASE, result );
}

void eclm_handle_file_update( WorkThread* t, EclmFileUpdate* data ) 
{
    EclmResultIn result;
    
    DEBUG_EE("nvmId=%d size=%d offset=%d seqNum=%d", 
        data->nvmId, 
        data->size, 
        data->offset, 
        data->seqNum
    );

    if ( data->data == NULL ) {
        ERROR("no data provided");
        // too bad there's not a "parse error" we could send
        result = ECLM_RES_I_FILE_ACCESS_ERR;
    } else {
        result = eclm_storage_write(
            data->nvmId, 
            data->size, 
            data->offset, 
            data->data
        );
    }
    
    eclm_send_ack_nack(&t->workState, ECLM_O_FILE_UPDATE, result );
}

void eclm_handle_file_request( WorkThread* t, EclmFileRequest *req ) 
{
    EclmResultIn result;
    EclmHostFileContent contentMsg;
    int block, readLen;
    
    void *data, *packet;
    int maxDataSize;
    Buffer b;
    
    DEBUG_EE("nvmId=%d seqNum=%d blockCount=%d", 
        req->nvmId, 
        req->seqNum, 
        req->blockCount
    );
    
    if ( gps_state_get_protocol( t->workState.gpsState ) == GPS_PROTO_OSP ) {
        maxDataSize = OSP_DATA_SIZE - 4;
        maxDataSize -= ( req->blockCount * 6 );
    } else {
        maxDataSize = (NMEA_DATA_SIZE/3) + 1;
    }
    
    data = malloc( maxDataSize );
    b = (Buffer) { data, maxDataSize, 0 };
    
    contentMsg.nvmId = req->nvmId;
    contentMsg.seqNum = req->seqNum;
    contentMsg.blockCount = req->blockCount;
    
    // collect blocks of data
    for (block=0; block<req->blockCount; block++) { 
        UINT16 blockSize = req->blockSizes[block];
        
        DEBUG_EE("block=%d size=%d offset=%d", 
            block, 
            blockSize, 
            req->blockOffsets[block]
        );
        
        if ( blockSize > 0 ) {
            packet = malloc( blockSize );
            
            result = eclm_storage_read(
                req->nvmId, 
                blockSize,
                req->blockOffsets[block],
                packet,
                &readLen
            );
        
            if (result != ECLM_RES_I_SUCCESS) {
                free( packet );
                goto end;
            }
        
            if ( blockSize != readLen ) {
                ERROR("block size mismatch: %d != %d", blockSize, readLen);
                free( packet );
                goto end;
            }
            
            if (FAILED( write_data( &b, packet, readLen ) )) {
                free( packet );
                goto end;
            }
            
            free( packet );
        }
    }
    
    // send message
    if ( b.pos == 0 ) {
        contentMsg.data = NULL;
    } else {
        int bsLen = req->blockCount * sizeof( UINT16 );
        contentMsg.blockSizes = malloc( bsLen );
        memcpy( contentMsg.blockSizes, req->blockSizes, bsLen );
        
        int boLen = req->blockCount * sizeof( UINT32 );
        contentMsg.blockOffsets = malloc( boLen );
        memcpy( contentMsg.blockOffsets, req->blockOffsets, boLen );
        
        contentMsg.data = malloc( b.pos );
        memcpy( contentMsg.data, b.data, b.pos );
    }
    
    eclm_send_host_file_content( &t->workState, &contentMsg );
    
end:
    free(data);
}

void eclm_handle_store_ee_header( WorkThread* t, OspStoreEeHeader* data ) 
{
    if ( mkdir( GPS_STORAGE_PATH, S_IRWXU ) == -1 ) {
        if ( errno != EEXIST ) {
            ERROR("error at mkdir: path=%s errno=%d %s", GPS_STORAGE_PATH, errno, strerror(errno));
            return;
        }
    }
    
    DEBUG_EE("offset=%d length=%d", data->dataOffset, data->dataLen);
    storage_write( ECLM_STORAGE_HEADER, data->dataLen, data->dataOffset, data->data );
}

void eclm_handle_fetch_ee_header( WorkThread* t ) 
{
    EclmResultOut result;
    OspEeHeaderResp obj;
    memset( &obj, 0, sizeof(obj) );
    
    while ( 1 ) {
        // malloc for each packet
        // data will be freed when message is freed
        obj.data = malloc( GPS_SGEE_MAX_BUFFER_SIZE );
        obj.dataLen = storage_read( ECLM_STORAGE_HEADER, GPS_SGEE_MAX_BUFFER_SIZE, obj.dataOffset, obj.data );
        
        if ( obj.dataLen <= 0 ) {
            free( obj.data );
            
            if ( obj.seqNum == 0 ) {                
                eclm_send_ack_nack(
                    &t->workState, 
                    OSP_ID_MINOR( OSP_ID_FETCH_EE_HEADER ), 
                    ECLM_RES_I_FILE_ACCESS_ERR 
                );
            }
            return;
        }
        
        obj.seqNum++;
        if (FAILED( eclm_send_ee_header_response( &t->workState, &obj, &result ) )) {
            ERROR("failed to send header response #%d result=%d", obj.seqNum, result);
            return;
        }
        obj.dataOffset += obj.dataLen;
    }
}

void eclm_handle_msg( WorkThread* t, Message *msg )
{
    switch (msg->id) 
    {
        case OSP_ID_DATA_AND_EPHEMERIS_REQ: {
            WARN("TODO: OSP_ID_DATA_AND_EPHEMERIS_REQ");
        } break;
        
        case OSP_ID_EE_INTEGRITY: {
            WARN("TODO: OSP_ID_EE_INTEGRITY");
        } break;
        
        case OSP_ID_EPHEMERIS_STATUS_RESP: {
            WARN("TODO: OSP_ID_EPHEMERIS_STATUS_RESP");
        } break;
        
        case OSP_ID_VERIFIED_BE_DATA: {
            WARN("TODO: OSP_ID_VERIFIED_BE_DATA");
        } break;
        
        case OSP_ID_ECLM_ACK_NACK: {
            EclmAckNackResponse* data = (EclmAckNackResponse*) msg->obj;
            WorkState *sgeeWorkState = &t->sgeeThread->workState;
            
            // pass some types of acks to sgee thread
            if ( sgeeWorkState->active ) {
                switch ( data->eclmId ) {
                    case ECLM_I_START_DL:
                    case ECLM_I_FILE_SIZE:
                    case ECLM_I_PACKET_DATA: {
                        message_queue_add( sgeeWorkState->readQueue, msg );
                    } return;
                }
            }
            
            eclm_handle_ack_nack(t, msg->obj);
        } break;
            
        case OSP_ID_EE_AGE_RESP:
            eclm_handle_ee_age_response(t, msg->obj);
            break;
            
        case OSP_ID_SGEE_AGE_RESP:
            eclm_handle_sgee_age_response(t, msg->obj);
            break;
            
        case OSP_ID_ECLM_DOWNLOAD_REQ:
            eclm_handle_sgee_request(t, msg->obj);
            break;
            
        case OSP_ID_HOST_FILE_ERASE:
            eclm_handle_file_erase(t, msg->obj);
            break;
            
        case OSP_ID_HOST_FILE_UPDATE:
            eclm_handle_file_update(t, msg->obj);
            break;
            
        case OSP_ID_HOST_FILE_REQ:
            eclm_handle_file_request(t, msg->obj);
            break;
        
        case OSP_ID_STORE_EE_HEADER:
            eclm_handle_store_ee_header(t, msg->obj);
            break;
        
        case OSP_ID_FETCH_EE_HEADER:
            eclm_handle_fetch_ee_header(t);
            break;
        
        case OSP_ID_SIF_AIDING_STATUS_RESP:
            WARN("TODO: OSP_ID_SIF_AIDING_STATUS_RESP");
            break;
        
        case OSP_ID_EE_ACK_NACK:
            WARN("TODO: OSP_ID_EE_ACK_NACK");
            break;
        
        default:
            WARN("unhandled ECLM message: 0x%x", msg->id);
            break;
    }
}
