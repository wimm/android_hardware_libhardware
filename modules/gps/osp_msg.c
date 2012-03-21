/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * OSP message
 *
 */

#include "osp_msg.h"

#include "eclm_types.h"
#include "patch_manager_types.h"


#define OSP_READ_INT(__b,__var,__len)   FAIL_UNLESS(READ_INT(__b,__var,__len))
#define OSP_READ_VAR(__b, __var)        FAIL_UNLESS(READ_VAR(__b, __var))

#define OSP_WRITE_INT(__b,__var,__len)  FAIL_UNLESS(WRITE_INT(__b,__var,__len))
#define OSP_WRITE_VAR(__b, __var)       FAIL_UNLESS(WRITE_VAR(__b, __var))

#define OSP_WRITE_DATA(__b,__data,__len) do {\
    if (FAILED( write_data( (__b), (__data), (__len) ) )) { \
        ERROR("failed to write " #__data); \
        return EXIT_FAILURE; \
    }} while (0)


void osp_msg_dispose( void* msg );

EXIT_CODE osp_msg_write_eclm( Message* msg, Buffer* buff ) 
{
    switch ( msg->id ) {
        case OSP_ID_ECLM_START_DOWNLOAD: {
            // no data
        } break;
        
        case OSP_ID_SGEE_DL_FILE_SIZE: {
            EclmSgeeDownloadFileSize* obj = (EclmSgeeDownloadFileSize*) msg->obj;
            OSP_WRITE_VAR( buff, obj->fileLength );
        } break;
        
        case OSP_ID_SGEE_DL_PKT_DATA: {
            EclmSgeeDownloadFileContent* obj = (EclmSgeeDownloadFileContent*) msg->obj;
            OSP_WRITE_VAR( buff, obj->seqNum );
            OSP_WRITE_VAR( buff, obj->dataLen );
            OSP_WRITE_DATA( buff, obj->data, obj->dataLen );
        } break;
        
        // TODO case OSP_ID_EE_AGE_REQ:
        
        case OSP_ID_SGEE_AGE_REQ: {
            EclmSgeeAgeRequest* obj = (EclmSgeeAgeRequest*) msg->obj;
            OSP_WRITE_VAR( buff, obj->satPrn );
        } break;
        
        case OSP_ID_HOST_FILE_CONTENT: {
            int dataLen = 0;
            EclmHostFileContent* obj = (EclmHostFileContent*) msg->obj;
            OSP_WRITE_VAR( buff, obj->seqNum );
            OSP_WRITE_VAR( buff, obj->nvmId );
            OSP_WRITE_VAR( buff, obj->blockCount );
            if ( obj->blockCount > 0 ) {
                int block;
                for (block=0; block<obj->blockCount; block++) {
                    OSP_WRITE_VAR( buff, obj->blockSizes[block] );
                    OSP_WRITE_VAR( buff, obj->blockOffsets[block] );
                    dataLen += obj->blockSizes[block];
                }
                OSP_WRITE_DATA( buff, obj->data, dataLen );
            }
        } break;
        
        case OSP_ID_HOST_ACK_NACK: {
            EclmAckNackResponse* obj = (EclmAckNackResponse*) msg->obj;
            OSP_WRITE_INT( buff, OSP_MID_EE_OUTPUT, 1 );
            OSP_WRITE_VAR( buff, obj->eclmId );
            OSP_WRITE_VAR( buff, obj->ackNack );
            OSP_WRITE_VAR( buff, obj->result );
        } break;
        
        case OSP_ID_EE_HEADER_RESP: {
            OspEeHeaderResp* obj = (OspEeHeaderResp*) msg->obj;
            OSP_WRITE_VAR( buff, obj->seqNum );
            OSP_WRITE_VAR( buff, obj->dataLen );
            OSP_WRITE_VAR( buff, obj->dataOffset );
            OSP_WRITE_DATA( buff, obj->data, obj->dataLen );
        } break;
        
        default: goto error;
    }
    
    return EXIT_SUCCESS;
    
error: 
    WARN("Unhandled message: 0x%02x/0x%04x", msg->type, msg->id);
    return EXIT_FAILURE;
}

EXIT_CODE osp_msg_write_osp( Message* msg, Buffer* buff ) 
{
    OspMajorId idMaj = 0;
    OspMinorId idMin = 0;
    
    // MID
    idMaj = OSP_ID_MAJOR( msg->id );
    OSP_WRITE_VAR( buff, idMaj );
    
    switch ( idMaj ) 
    {
        case OSP_MID_SET_NMEA_MODE: {
            OspSetNmeaMode* obj = (OspSetNmeaMode*) msg->obj;
            OSP_WRITE_VAR( buff, obj->debugMode );
            OSP_WRITE_VAR( buff, obj->ggaRate );
            OSP_WRITE_INT( buff, 1, 1 );  // chksum
            OSP_WRITE_VAR( buff, obj->gllRate );
            OSP_WRITE_INT( buff, 1, 1 );  // chksum
            OSP_WRITE_VAR( buff, obj->gsaRate );
            OSP_WRITE_INT( buff, 1, 1 );  // chksum
            OSP_WRITE_VAR( buff, obj->gsvRate );
            OSP_WRITE_INT( buff, 1, 1 );  // chksum
            OSP_WRITE_VAR( buff, obj->rmcRate );
            OSP_WRITE_INT( buff, 1, 1 );  // chksum
            OSP_WRITE_VAR( buff, obj->vtgRate );
            OSP_WRITE_INT( buff, 1, 1 );  // chksum
            OSP_WRITE_VAR( buff, obj->mssRate );
            OSP_WRITE_INT( buff, 1, 1 );  // chksum
            OSP_WRITE_VAR( buff, obj->epeRate );
            OSP_WRITE_INT( buff, 1, 1 );  // chksum
            OSP_WRITE_VAR( buff, obj->zdaRate );
            OSP_WRITE_INT( buff, 1, 1 );  // chksum
            OSP_WRITE_INT( buff, 0, 1 );  // unused
            OSP_WRITE_INT( buff, 0, 1 );  // unused
            OSP_WRITE_VAR( buff, obj->baudRate );
        } break;
        
        case OSP_MID_POLL_SW_VERSION: {
            OSP_WRITE_INT( buff, 0, 1 );
        } break;
        
        case OSP_MID_SW_TOOLBOX: 
        {
            // SID
            idMin = OSP_ID_MINOR( msg->id );
            OSP_WRITE_VAR( buff, idMin );
            
            switch ( msg->id ) 
            {
                case OSP_ID_PM_LOAD_REQ: {
                    PatchManagerLoadRequest* obj = (PatchManagerLoadRequest*) msg->obj;
                    OSP_WRITE_VAR( buff, obj->seqNumber );
                    if ( obj->seqNumber == 1 ) {
                        OSP_WRITE_INT( buff, 'P', 1 );
                        OSP_WRITE_INT( buff, 'M', 1 );
                        OSP_WRITE_VAR( buff, obj->romVersion );
                        OSP_WRITE_VAR( buff, obj->patchVersion );
                        OSP_WRITE_VAR( buff, obj->patchDataBaseAddr );
                        OSP_WRITE_VAR( buff, obj->patchDataLength );
                        OSP_WRITE_VAR( buff, obj->patchDataRamOffset );
                    }
                    OSP_WRITE_DATA( buff, obj->data, obj->dataLen );
                } break;
                
                case OSP_ID_PM_EXIT_REQ:        break;   // no data
                case OSP_ID_PM_START_REQ:       break;   // no data
                
                default: goto error;
            }
        } break;
        
        case OSP_MID_EE_INPUT:
        {
            // SID
            idMin = OSP_ID_MINOR( msg->id );
            OSP_WRITE_VAR( buff, idMin );
            
            FAIL_UNLESS( osp_msg_write_eclm( msg, buff ) );
        } break;
        
        default: goto error;
    }
    
    return EXIT_SUCCESS;
    
error: 
    WARN("Unhandled message: 0x%02x/0x%04x", msg->type, msg->id);
    return EXIT_FAILURE;
}

EXIT_CODE osp_msg_write_payload( Message* msg, Buffer* buff, UINT16* chksum ) 
{
    UINT32 i;
    UINT32 payloadChksum = 0;
    
    switch ( msg->type )
    {
        case MSG_TYPE_OSP: 
            FAIL_UNLESS( osp_msg_write_osp( msg, buff ) );
            break;
        
        default:
            WARN("Unhandled message: 0x%02x/0x%04x", msg->type, msg->id);
            return EXIT_FAILURE;
    }
    
    // calc chksum
    for ( i=0; i<buff->pos; i++ ) {
        payloadChksum += buff->data[i];
    }
    payloadChksum &= 0x7FFF;
    if ( chksum ) *chksum = payloadChksum;
    
    return EXIT_SUCCESS;
}

EXIT_CODE osp_msg_write( Message* msg, Buffer* buff ) 
{
    Buffer payload;
    UINT16 chksum;
    UINT8 buf[OSP_DATA_SIZE];

    DEBUG_MSG_IO("0x%02x/0x%04x", msg->type, msg->id);

    payload = (Buffer){ buf, OSP_DATA_SIZE, 0 };
    FAIL_UNLESS( osp_msg_write_payload( msg, &payload, &chksum ) );
    
    // start bytes 0xa0 0xa2
    FAIL_UNLESS( WRITE_INT( buff, OSP_START_BYTE_1, 1 ) );
    FAIL_UNLESS( WRITE_INT( buff, OSP_START_BYTE_2, 1 ) );
    
    // payload length
    FAIL_UNLESS( WRITE_INT( buff, payload.pos, 2 ) );
    
    // payload
    if (FAILED( write_data( buff, payload.data, payload.pos ) )) {
        ERROR("failed to copy payload data");
        return EXIT_FAILURE;
    }
    
    // checksum
    FAIL_UNLESS( WRITE_INT( buff, chksum, 2 ) );
    
    // end bytes 0xb0 0xb3
    FAIL_UNLESS( WRITE_INT( buff, OSP_END_BYTE_1, 1 ) );
    FAIL_UNLESS( WRITE_INT( buff, OSP_END_BYTE_2, 1 ) );
    
#if DBG_OSP
    DEBUG("%d bytes", buff->pos);
    PRINT_DATA( buff->data, buff->pos );
#endif

    return EXIT_SUCCESS;
}

EXIT_CODE osp_msg_read_eclm( Message* msg, Buffer* buff ) 
{
    switch ( msg->id )
    {
        case OSP_ID_DATA_AND_EPHEMERIS_REQ:
        case OSP_ID_EE_INTEGRITY:
        case OSP_ID_EPHEMERIS_STATUS_RESP:
        case OSP_ID_VERIFIED_BE_DATA:
            // TODO
            break;
        
        case OSP_ID_ECLM_ACK_NACK: {
            EclmAckNackResponse obj;
            buff->pos += 1;
            OSP_READ_VAR( buff, obj.eclmId );
            OSP_READ_VAR( buff, obj.ackNack );
            OSP_READ_VAR( buff, obj.result );
            msg->obj = COPY( obj );
        } break;
            
        case OSP_ID_EE_AGE_RESP: {
            EclmEeAgeResponse obj;
            OSP_READ_VAR( buff, obj.count );
            
            if ( obj.count == 0 ) {
                obj.satAges = NULL;
            } else {
                int idx;
                obj.satAges = (EclmEeAge*) calloc( obj.count, sizeof( EclmEeAge ) );
                for ( idx = 0; idx < obj.count; idx++ ) {
                    if (FAILED(READ_VAR( buff, obj.satAges[idx].prnNum )) ||
                        FAILED(READ_VAR( buff, obj.satAges[idx].ephPos )) ||
                        FAILED(READ_VAR( buff, obj.satAges[idx].eePosAge )) ||
                        FAILED(READ_VAR( buff, obj.satAges[idx].cgeePosGPSWeek )) ||
                        FAILED(READ_VAR( buff, obj.satAges[idx].cgeePosTOE )) ||
                        FAILED(READ_VAR( buff, obj.satAges[idx].ephClkFlag )) ||
                        FAILED(READ_VAR( buff, obj.satAges[idx].eeClkAge )) ||
                        FAILED(READ_VAR( buff, obj.satAges[idx].cgeeClkGPSWeek )) ||
                        FAILED(READ_VAR( buff, obj.satAges[idx].cgeeClkTOE ))
                    ) {
                        free( obj.satAges );
                        return EXIT_FAILURE;
                    }
                }
            }
            
            msg->obj = COPY( obj );
        } break;
            
        case OSP_ID_SGEE_AGE_RESP: {
            EclmSgeeAgeResponse obj;
            OSP_READ_VAR( buff, obj.age );
            OSP_READ_VAR( buff, obj.interval );
            msg->obj = COPY( obj );
        } break;
            
        case OSP_ID_ECLM_DOWNLOAD_REQ: {
            EclmSgeeRequest obj;
            OSP_READ_VAR( buff, obj.start );
            OSP_READ_VAR( buff, obj.delay );
            msg->obj = COPY( obj );
        } break;
            
        case OSP_ID_HOST_FILE_ERASE: {
            EclmFileErase obj;
            OSP_READ_VAR( buff, obj.nvmId );
            msg->obj = COPY( obj );
        } break;
            
        case OSP_ID_HOST_FILE_UPDATE: {
            EclmFileUpdate obj;
            OSP_READ_VAR( buff, obj.nvmId );
            OSP_READ_VAR( buff, obj.size );
            OSP_READ_VAR( buff, obj.offset );
            OSP_READ_VAR( buff, obj.seqNum );
            
            if ( obj.size == 0 ) {
                obj.data = NULL;
            } else {
                obj.data = malloc( obj.size );
                if (FAILED( read_data( buff, obj.data, obj.size ) )) {
                    ERROR("failed to read file update data");
                    free( obj.data );
                    return EXIT_FAILURE;
                }
            }
            
            msg->obj = COPY( obj );
        } break;
            
        case OSP_ID_HOST_FILE_REQ: {
            EclmFileRequest obj;
            OSP_READ_VAR( buff, obj.nvmId );
            OSP_READ_VAR( buff, obj.seqNum );
            OSP_READ_VAR( buff, obj.blockCount );
            
            if ( obj.blockCount == 0 ) {
                obj.blockSizes = NULL;
                obj.blockOffsets = NULL;
            } else {
                int idx;
                
                obj.blockSizes = (UINT16*) calloc( obj.blockCount, sizeof( UINT16 ) );
                obj.blockOffsets = (UINT32*) calloc( obj.blockCount, sizeof( UINT32 ) );
                
                for ( idx = 0; idx < obj.blockCount; idx++ ) {
                    if (FAILED( READ_VAR( buff, obj.blockSizes[idx] ) )) {
                        free( obj.blockSizes );
                        free( obj.blockOffsets );
                        return EXIT_FAILURE;
                    }
                    if (FAILED( READ_VAR( buff, obj.blockOffsets[idx] ) )) {
                        free( obj.blockSizes );
                        free( obj.blockOffsets );
                        return EXIT_FAILURE;
                    }
                }
            }
            
            msg->obj = COPY( obj );
        } break;
                
        case OSP_ID_STORE_EE_HEADER: {
            OspStoreEeHeader obj;
            OSP_READ_VAR( buff, obj.dataOffset );
            OSP_READ_VAR( buff, obj.dataLen );
            if ( obj.dataLen == 0 ) {
                obj.data = NULL;
            } else {
                obj.data = malloc( obj.dataLen );
                if (FAILED( read_data( buff, obj.data, obj.dataLen ) )) {
                    ERROR("failed to read ee header data");
                    free( obj.data );
                    return EXIT_FAILURE;
                }
            }
            msg->obj = COPY( obj );
        } break;
        
        case OSP_ID_FETCH_EE_HEADER: {
            // no data
        } break;
        
        case OSP_ID_SIF_AIDING_STATUS_RESP:
        case OSP_ID_EE_ACK_NACK: 
            // TODO
            break;
            
        default:
            WARN("unknown ECLM message: 0x%x", msg->id);
            return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

EXIT_CODE osp_msg_read( Message* msg, Buffer* buff ) 
{
    OspMajorId idMaj = 0;
    OspMinorId idMin = 0;
    
    msg->type = MSG_TYPE_OSP;
    msg->dispose = osp_msg_dispose;
    
    // MID
    OSP_READ_VAR( buff, idMaj );
    
    switch ( idMaj ) 
    {
        case OSP_MID_MEASURED_NAVIGATION_DATA: {
            OspMeasuredNavigationData obj;
            int i;
            memset( &obj, 0, sizeof(obj) );
            OSP_READ_VAR( buff, obj.xPos );
            OSP_READ_VAR( buff, obj.yPos );
            OSP_READ_VAR( buff, obj.zPos );
            OSP_READ_VAR( buff, obj.xVel );
            OSP_READ_VAR( buff, obj.yVel );
            OSP_READ_VAR( buff, obj.zVel );
            OSP_READ_VAR( buff, obj.mode1 );
            OSP_READ_VAR( buff, obj.hdop2 );
            OSP_READ_VAR( buff, obj.mode2 );
            OSP_READ_VAR( buff, obj.gpsWeek );
            OSP_READ_VAR( buff, obj.gpsTimeOfWeek );
            OSP_READ_VAR( buff, obj.svsUsedInFix );
            for (i=0; i<GPS_CHANNEL_MAX; i++) {
                READ_VAR( buff, obj.chPrn[i] );
            }
            msg->id = OSP_ID( idMaj, 0 );
            msg->obj = COPY( obj );
        } break;
        
        case OSP_MID_MEASURED_TRACKER_DATA: {
            OspMeasuredTrackerData obj;
            int i, j;
            memset( &obj, 0, sizeof(obj) );
            OSP_READ_VAR( buff, obj.gpsWeek );
            OSP_READ_VAR( buff, obj.gpsTimeOfWeek );
            OSP_READ_VAR( buff, obj.channels );
            for (i=0; i<obj.channels; i++) {
                OSP_READ_VAR( buff, obj.channel[i].prn );
                OSP_READ_VAR( buff, obj.channel[i].azimuth );
                OSP_READ_VAR( buff, obj.channel[i].elevation );
                OSP_READ_VAR( buff, obj.channel[i].state );
                for (j=0; j<10; j++) {
                    OSP_READ_VAR( buff, obj.channel[i].snr[j] );
                }
            }
            msg->id = OSP_ID( idMaj, 0 );
            msg->obj = COPY( obj );
        } break;
    
        case OSP_MID_SW_VERSION: {
            OspSwVersion obj;
            UINT8 sirfLen, custLen;
            
            OSP_READ_VAR( buff, sirfLen );
            OSP_READ_VAR( buff, custLen );
            
            if ( sirfLen > 0 ) {
                obj.sirfVersion = (char*) malloc( sirfLen );
                if (FAILED( read_data( buff, (void*)obj.sirfVersion, sirfLen ) )) {
                    ERROR("failed to read sirfVersion");
                    free( obj.sirfVersion );
                    return EXIT_FAILURE;
                }
            } else {
                obj.sirfVersion = NULL;
            }
            
            if ( sirfLen > 0 ) {
                obj.customerVersion = (char*) malloc( custLen );
                if (FAILED( read_data( buff, (void*)obj.customerVersion, custLen ) )) {
                    ERROR("failed to read customerVersion");
                    free( obj.sirfVersion );
                    free( obj.customerVersion );
                    return EXIT_FAILURE;
                }
            } else {
                obj.customerVersion = NULL;
            }
            
            msg->id = OSP_ID( idMaj, 0 );
            msg->obj = COPY( obj );
        } break;

        case OSP_MID_COMMAND_ACK: {
            OspCommandAck obj;
            OspMajorId mid = 0;
            OspMinorId sid = 0;
            OSP_READ_VAR( buff, mid );
            READ_VAR( buff, sid );  // optional
            obj.msgId = OSP_ID( mid, sid );
            msg->id = OSP_ID( idMaj, 0 );
            msg->obj = COPY( obj );
        } break;

        case OSP_MID_COMMAND_NACK: {
            OspCommandNack obj;
            OspMajorId mid = 0;
            OspMinorId sid = 0;
            OSP_READ_VAR( buff, mid );
            READ_VAR( buff, sid );  // optional
            obj.msgId = OSP_ID( mid, sid );
            msg->id = OSP_ID( idMaj, 0 );
            msg->obj = COPY( obj );
        } break;

        case OSP_MID_OK_TO_SEND: {
            OspOkToSend obj;
            OSP_READ_VAR( buff, obj.okToSend );
            msg->id = OSP_ID( idMaj, 0 );
            msg->obj = COPY( obj );
        } break;
        
        case OSP_MID_EE_OUTPUT: {
            OSP_READ_VAR( buff, idMin );
            msg->id = OSP_ID( idMaj, idMin );
            FAIL_UNLESS( osp_msg_read_eclm( msg, buff ) );
        } break;
    
        case OSP_MID_SW_TOOLBOX: {
            OSP_READ_VAR( buff, idMin );
            msg->id = OSP_ID( idMaj, idMin );
            switch ( msg->id ) 
            {
                case OSP_ID_PM_PROMPT: {
                    PatchManagerPrompt obj;
                    OSP_READ_VAR( buff, obj.chipId );
                    OSP_READ_VAR( buff, obj.siliconId );
                    OSP_READ_VAR( buff, obj.romVersion );
                    OSP_READ_VAR( buff, obj.patchVersion );
                    msg->obj = COPY( obj );
                } break;
                
                case OSP_ID_PM_ACK: {
                    PatchManagerAck obj;
                    OSP_READ_VAR( buff, obj.seqNumber );
                    OSP_READ_VAR( buff, obj.minorId );
                    OSP_READ_VAR( buff, obj.status );
                    msg->obj = COPY( obj );
                } break;
            }
        } break;
            
        default:
            msg->id = OSP_ID( idMaj, 0 );
            break;
    }
    
    DEBUG_MSG_IO("0x%02x/0x%04x", msg->type, msg->id);
    return EXIT_SUCCESS;
}

void osp_msg_dispose( void* msg )
{
    void* mObj = ((Message*)msg)->obj;
    if ( mObj == NULL ) return;
    
    switch ( ((Message*)msg)->id ) 
    {
        case OSP_ID_SW_VERSION: {
            OspSwVersion* obj = (OspSwVersion*) mObj;
            if ( obj->sirfVersion ) free( obj->sirfVersion );
            if ( obj->customerVersion ) free( obj->customerVersion );
        } break;
            
        case OSP_ID_PM_LOAD_REQ: {
            PatchManagerLoadRequest* obj = (PatchManagerLoadRequest*) mObj;
            if ( obj->data ) free( obj->data );
        } break;

        case OSP_ID_SGEE_DL_PKT_DATA: {
            EclmSgeeDownloadFileContent* obj = (EclmSgeeDownloadFileContent*) mObj;
            if ( obj->data ) free( obj->data );
        } break;
            
        case OSP_ID_HOST_FILE_CONTENT: {
            EclmHostFileContent* obj = (EclmHostFileContent*) mObj;
            if ( obj->blockSizes ) free( obj->blockSizes );
            if ( obj->blockOffsets ) free( obj->blockOffsets );
            if ( obj->data ) free( obj->data );
        } break;
            
        case OSP_ID_EE_AGE_RESP: {
            EclmEeAgeResponse* obj = (EclmEeAgeResponse*) mObj;
            if ( obj->satAges ) free( obj->satAges );
        } break;
            
        case OSP_ID_HOST_FILE_UPDATE: {
            EclmFileUpdate* obj = (EclmFileUpdate*) mObj;
            if ( obj->data ) free( obj->data );
        } break;
            
        case OSP_ID_HOST_FILE_REQ: {
            EclmFileRequest* obj = (EclmFileRequest*) mObj;
            if ( obj->blockSizes ) free( obj->blockSizes );
            if ( obj->blockOffsets ) free( obj->blockOffsets );
        } break;
            
        case OSP_ID_STORE_EE_HEADER: {
            OspStoreEeHeader* obj = (OspStoreEeHeader*) mObj;
            if ( obj->data ) free( obj->data );
        } break;
            
        case OSP_ID_EE_HEADER_RESP: {
            OspEeHeaderResp* obj = (OspEeHeaderResp*) mObj;
            if ( obj->data ) free( obj->data );
        } break;
    }
}
