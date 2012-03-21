/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * NMEA message
 *
 */

#include "nmea_msg.h"
#include "nmea_tokenizer.h"

#include "eclm_types.h"
#include "nmea_types.h"
#include "osp_types.h"


#define NMEA_WRITE_FORMAT(_buf, _fmt, _arg...) \
    FAIL_UNLESS( write_format(_buf, _fmt, ## _arg) )

#define NMEA_WRITE_DATA(_buf, _data, _length) \
({ \
    int __idx; \
    for (__idx=0; __idx<_length; __idx++) { \
        NMEA_WRITE_FORMAT(_buf, ",%x", _data[__idx]); \
    } \
})


EXIT_CODE nmea_msg_write_eclm( Message* msg, Buffer* buff ) 
{
    NMEA_WRITE_FORMAT(
        buff, "%s%d", 
        NMEA_SIRF_MSG_PREFIX, 
        SIRF_I_EE_ECLM_MSG
    );
    
    switch ( msg->id ) {
        case OSP_ID_ECLM_START_DOWNLOAD: {
            // trailing ",0" not mentioned in NMEA spec
            // but seems to be required (see SiRF protocol_mgr.c)
            NMEA_WRITE_FORMAT(
                buff, ",%x,0",
                ECLM_I_START_DL
            );
        } break;
        
        case OSP_ID_SGEE_DL_FILE_SIZE: {
            EclmSgeeDownloadFileSize* obj = (EclmSgeeDownloadFileSize*) msg->obj;
            NMEA_WRITE_FORMAT(
                buff, ",%x,%x",
                ECLM_I_FILE_SIZE,
                obj->fileLength
            );
        } break;
        
        case OSP_ID_SGEE_DL_PKT_DATA: {
            EclmSgeeDownloadFileContent* obj = (EclmSgeeDownloadFileContent*) msg->obj;
            NMEA_WRITE_FORMAT(
                buff, ",%x,%hu,%hu",
                ECLM_I_PACKET_DATA,
                obj->seqNum, obj->dataLen
            );
            NMEA_WRITE_DATA( buff, obj->data, obj->dataLen );
        } break;
        
        case OSP_ID_SGEE_AGE_REQ: {
            EclmSgeeAgeRequest* obj = (EclmSgeeAgeRequest*) msg->obj;
            NMEA_WRITE_FORMAT(
                buff, ",%x,%x",
                ECLM_I_SGEE_AGE_REQ,
                obj->satPrn
            );
        } break;
        
        case OSP_ID_HOST_FILE_CONTENT: {
            int dataLen = 0;
            EclmHostFileContent* obj = (EclmHostFileContent*) msg->obj;
            NMEA_WRITE_FORMAT(
                buff, ",%x,%x,%x,%x",
                ECLM_I_HOST_FILE,
                obj->seqNum, 
                obj->nvmId, 
                obj->blockCount
            );
            if ( obj->blockCount > 0 ) {
                int block;
                for (block=0; block<obj->blockCount; block++) {
                    NMEA_WRITE_FORMAT(
                        buff, ",%x",
                        obj->blockSizes[block]
                    );
                    dataLen += obj->blockSizes[block];
                }
                for (block=0; block<obj->blockCount; block++) {
                    NMEA_WRITE_FORMAT(
                        buff, ",%x",
                        obj->blockOffsets[block]
                    );
                }
                NMEA_WRITE_DATA( buff, obj->data, dataLen );
            }
        } break;
        
        case OSP_ID_HOST_ACK_NACK: {
            EclmAckNackResponse* obj = (EclmAckNackResponse*) msg->obj;
            NMEA_WRITE_FORMAT(
                buff, ",%x,%x,%x,%x,%x",
                ECLM_I_ACK_NACK,
                SIRF_O_EE_ECLM_MSG, 
                obj->eclmId, obj->ackNack, obj->result
            );
        } break;
        
        default: goto error;
    }
    
    return EXIT_SUCCESS;
    
error: 
    WARN("Unhandled message: 0x%02x/0x%04x", msg->type, msg->id);
    return EXIT_FAILURE;
}

EXIT_CODE nmea_msg_write_sirf( Message* msg, Buffer* buff ) 
{
    NMEA_WRITE_FORMAT(
        buff, "%s%d", 
        NMEA_SIRF_MSG_PREFIX, 
        msg->id
    );
    
    switch ( msg->id )
    {
        case SIRF_I_SET_SERIAL_PORT: {
            SirfSerialConfig* obj = (SirfSerialConfig*) msg->obj;
            NMEA_WRITE_FORMAT(
                buff, ",%d,%d,%d,%d,%d", 
                obj->protocol, obj->bitRate, 8, 1, 0
            );
        } break;
        
        case SIRF_I_LLA_NAV_INIT: {
            SirfLlaNavInit* obj = (SirfLlaNavInit*) msg->obj;
            float lat = 0.0f, lon = 0.0f;
            int alt = 0;
            
            if ( ( obj->location.flags & GPS_LOCATION_HAS_LAT_LONG ) != 0 ) {
                lat = obj->location.latitude;
                lon = obj->location.longitude;
            }
            
            if ( ( obj->location.flags & GPS_LOCATION_HAS_ALTITUDE ) != 0 ) {
                alt = obj->location.altitude;
            }
            
            NMEA_WRITE_FORMAT(
                buff, ",%f,%f,%d,%d,%d,%d,%d,%d", 
                lat, lon, alt, 
                obj->clockDrift, 
                obj->dateTime.timeOfWeek, 
                obj->dateTime.weekNumber,
                obj->channelCount, 
                obj->resetMode
            );
        } break;
        
        default:
            ERROR("Unhandled message: 0x%02x/0x%04x", msg->type, msg->id);
            return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

EXIT_CODE nmea_msg_write_osp( Message* msg, Buffer* buff ) 
{
    switch ( OSP_ID_MAJOR(msg->id) )
    {
        case OSP_MID_EE_INPUT: 
            FAIL_UNLESS( nmea_msg_write_eclm( msg, buff ) );
            break;
        
        default:
            ERROR("Unhandled message: 0x%02x/0x%04x", msg->type, msg->id);
            return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

EXIT_CODE nmea_msg_write_payload( Message* msg, Buffer* buff, UINT8* chksum ) 
{
    UINT32 i;
    UINT8 payloadChksum = 0;

    switch ( msg->type ) 
    {
        case MSG_TYPE_SIRF:
            FAIL_UNLESS( nmea_msg_write_sirf( msg, buff ) );
            break;
            
        case MSG_TYPE_OSP:
            FAIL_UNLESS( nmea_msg_write_osp( msg, buff ) );
            break;
        
        default:
            WARN("Unhandled message: 0x%02x/0x%04x", msg->type, msg->id);
            return EXIT_FAILURE;
    }
    
    // calc chksum
    for ( i=0; i<buff->pos; i++ ) {
        payloadChksum ^= buff->data[i];
    }
    if ( chksum ) *chksum = payloadChksum;
    
    return EXIT_SUCCESS;
}

EXIT_CODE nmea_msg_write( Message* msg, Buffer* buff ) 
{
    Buffer payload;
    UINT8 chksum;
    UINT8 buf[NMEA_DATA_SIZE+1];

    DEBUG_MSG_IO("0x%02x/0x%04x", msg->type, msg->id);

    payload = (Buffer){ buf, NMEA_DATA_SIZE+1, 0 };
    FAIL_UNLESS( nmea_msg_write_payload( msg, &payload, &chksum ) );
    
    if (FAILED( write_format( buff, 
        "$%.*s*%02x\r\n",
        payload.pos, payload.data, chksum
    ) )) {
        ERROR("failed to format NMEA");
        return EXIT_FAILURE;
    }

#if DBG_NMEA
    DEBUG("%.*s", buff->pos, buff->data);
#endif

    return EXIT_SUCCESS;
}

EXIT_CODE nmea_msg_read_eclm( Message* msg, NmeaTokenizer* tzer, EclmMessageId eclmId )
{
    msg->id = OSP_ID( OSP_MID_EE_OUTPUT, eclmId );
    
    switch (eclmId)
    {
        case ECLM_O_ACK_NACK: {
            EclmAckNackResponse ackNackResponse;
            
            TKN_HEX( tzer, 3, ackNackResponse.eclmId );
            TKN_HEX( tzer, 4, ackNackResponse.ackNack );
            TKN_HEX( tzer, 5, ackNackResponse.result );
            
            msg->obj = COPY( ackNackResponse );
        } break;
            
        case ECLM_O_EE_AGE_RESP: {
            EclmEeAgeResponse obj;
            int idx, tkIdx = 2;
            
            TKN_INT( tzer, tkIdx++, obj.count );
            
            if ( obj.count == 0 ) {
                obj.satAges = NULL;
            } else {
                int idx;
                obj.satAges = (EclmEeAge*) calloc( obj.count, sizeof( EclmEeAge ) );
                for ( idx = 0; idx < obj.count; idx++ ) {
                    if (FAILED(TKN_HEX( tzer, tkIdx++, obj.satAges[idx].prnNum )) ||
                        FAILED(TKN_HEX( tzer, tkIdx++, obj.satAges[idx].ephPos )) ||
                        FAILED(TKN_HEX( tzer, tkIdx++, obj.satAges[idx].eePosAge )) ||
                        FAILED(TKN_HEX( tzer, tkIdx++, obj.satAges[idx].cgeePosGPSWeek )) ||
                        FAILED(TKN_HEX( tzer, tkIdx++, obj.satAges[idx].cgeePosTOE )) ||
                        FAILED(TKN_HEX( tzer, tkIdx++, obj.satAges[idx].ephClkFlag )) ||
                        FAILED(TKN_HEX( tzer, tkIdx++, obj.satAges[idx].eeClkAge )) ||
                        FAILED(TKN_HEX( tzer, tkIdx++, obj.satAges[idx].cgeeClkGPSWeek )) ||
                        FAILED(TKN_HEX( tzer, tkIdx++, obj.satAges[idx].cgeeClkTOE ))
                    ) {
                        free( obj.satAges );
                        return EXIT_FAILURE;
                    }
                }
            }
            
            msg->obj = COPY( obj );
        } break;
            
        case ECLM_O_SGEE_AGE_RESP: {
            EclmSgeeAgeResponse sgeeAgeResponse;
            TKN_HEX( tzer, 2, sgeeAgeResponse.age );
            TKN_HEX( tzer, 3, sgeeAgeResponse.interval );
            
            msg->obj = COPY( sgeeAgeResponse );
        } break;
            
        case ECLM_O_SGEE_REQ: {
            EclmSgeeRequest sgeeRequest;
            TKN_HEX( tzer, 2, sgeeRequest.start );
            TKN_HEX( tzer, 3, sgeeRequest.delay );
            
            msg->obj = COPY( sgeeRequest );
        } break;
            
        case ECLM_O_FILE_ERASE: {
            EclmFileErase fileErase;
            TKN_HEX( tzer, 2, fileErase.nvmId );
            
            msg->obj = COPY( fileErase );
        } break;
            
        case ECLM_O_FILE_UPDATE: {
            EclmFileUpdate obj;
            TKN_HEX( tzer, 2, obj.nvmId );
            TKN_HEX( tzer, 3, obj.size );
            TKN_HEX( tzer, 4, obj.offset );
            TKN_HEX( tzer, 5, obj.seqNum );
            
            if ( obj.size == 0 ) {
                obj.data = NULL;
            } else {
                obj.data = malloc( obj.size );
                if (FAILED( nmea_tokenizer_get_data( tzer, 6, obj.data, obj.size ) )) {
                    ERROR("failed to read file update data");
                    free( obj.data );
                    return EXIT_FAILURE;
                }
            }
            
            msg->obj = COPY( obj );
        } break;
            
        case ECLM_O_FILE_REQ: {
            EclmFileRequest obj;
            int tkIdx = 2;
            
            TKN_HEX( tzer, tkIdx++, obj.nvmId );
            TKN_HEX( tzer, tkIdx++, obj.seqNum );
            TKN_HEX( tzer, tkIdx++, obj.blockCount );
            
            if ( obj.blockCount == 0 ) {
                obj.blockSizes = NULL;
                obj.blockOffsets = NULL;
            } else {
                int idx;
                
                obj.blockSizes = (UINT16*) calloc( obj.blockCount, sizeof( UINT16 ) );
                for ( idx = 0; idx < obj.blockCount; idx++ ) {
                    if (FAILED( TKN_HEX( tzer, tkIdx++, obj.blockSizes[idx] ) )) {
                        free( obj.blockSizes );
                        return EXIT_FAILURE;
                    }
                }
                
                obj.blockOffsets = (UINT32*) calloc( obj.blockCount, sizeof( UINT32 ) );
                for ( idx = 0; idx < obj.blockCount; idx++ ) {
                    if (FAILED( TKN_HEX( tzer, tkIdx++, obj.blockOffsets[idx] ) )) {
                        free( obj.blockSizes );
                        free( obj.blockOffsets );
                        return EXIT_FAILURE;
                    }
                }
            }
            
            msg->obj = COPY( obj );
        } break;
            
        default:
            WARN("unknown ECLM message: 0x%x", msg->id);
            nmea_tokenizer_print( tzer );
            return EXIT_FAILURE;
    }
    
    DEBUG_MSG_IO("0x%02x/0x%04x", msg->type, msg->id);
    return EXIT_SUCCESS;
}

EXIT_CODE nmea_msg_read_sirf( Message* msg, NmeaTokenizer* tzer )
{
    UINT32 id = 0;
    
    Token tok = nmea_tokenizer_get(tzer, 0);
    tok.p += strlen(NMEA_SIRF_MSG_PREFIX); // skip PSRF
    tok.length -= strlen(NMEA_SIRF_MSG_PREFIX);
    
    // ignore debugging messages
    if ( !memcmp(tok.p, "TXT", 3) ) 
        return EXIT_FAILURE;
    
    // translate id into OSP ID
    msg->type = MSG_TYPE_OSP;
    msg->dispose = osp_msg_dispose;
    
    if (FAILED( getInt( tok, 10, &id ) )) {
        ERROR("invalid MessageID");
        return EXIT_FAILURE;
    }
    
    switch (id) 
    {
        case SIRF_O_OK_TO_SEND: {
            OspOkToSend okToSend;
            TKN_INT( tzer, 1, okToSend.okToSend );
            
            msg->id = OSP_ID_OK_TO_SEND;
            msg->obj = COPY( okToSend );
        } break;
            
        case SIRF_O_DATA_AND_EE_REQ: {
            SirfEeRequest eeRequest;
            TKN_INT( tzer, 1, eeRequest.gpsTimeValidity );
            TKN_INT( tzer, 2, eeRequest.gpsWeek );
            getFloat( nmea_tokenizer_get(tzer, 3), &eeRequest.gpsTimeOfWeek );
            TKN_HEX( tzer, 4, eeRequest.ephReqMask );
            
            msg->id = OSP_ID_DATA_AND_EPHEMERIS_REQ;
            msg->obj = COPY( eeRequest );
        } break;
            
        case SIRF_O_EE_INTEGRITY: {
            SirfEeIntegrity eeIntegrity;
            TKN_HEX( tzer, 1, eeIntegrity.satPosValidity );
            TKN_HEX( tzer, 2, eeIntegrity.satClkValidity );
            TKN_HEX( tzer, 3, eeIntegrity.satHealth );
            
            msg->id = OSP_ID_EE_INTEGRITY;
            msg->obj = COPY( eeIntegrity );
        } break;
            
        case SIRF_O_EE_ACK: {
            SirfEeAck eeAck;
            TKN_INT( tzer, 1, eeAck.ackMsgId );
             
            msg->id = OSP_ID_EE_ACK_NACK;
            msg->obj = COPY( eeAck );
        } break;
            
        case SIRF_O_EE_ECLM_MSG: {
            EclmMessageId eclmId = 0;
            FAIL_UNLESS( TKN_HEX( tzer, 1, eclmId ) );
            return nmea_msg_read_eclm(msg, tzer, eclmId);
        } break;
            
        default:
            WARN("unknown PSRF message: %d", id);
            nmea_tokenizer_print( tzer );
            return EXIT_FAILURE;
    }
    
    DEBUG_MSG_IO("0x%02x/0x%04x", msg->type, msg->id);
    return EXIT_SUCCESS;
}

EXIT_CODE nmea_msg_read_geo( Message* msg, NmeaTokenizer* tzer ) 
{
    Token tok = nmea_tokenizer_get(tzer, 0);
    tok.p += strlen(NMEA_GEO_MSG_PREFIX); // skip GP
    tok.length -= strlen(NMEA_GEO_MSG_PREFIX);
    
    msg->type = MSG_TYPE_GEO;
    
    if ( !memcmp(tok.p, "GGA", 3) ) {
        NmeaGeoGGA gga;
        
        msg->id = NMEA_GEO_GGA;
        
        // check for empty msg
        // allow msg to pass thru so we know chip has powered on
        if (FAILED( getTime( nmea_tokenizer_get(tzer, 1), &gga.time ) )) {
            goto end;
        }
        
        getLatLon( 
            nmea_tokenizer_get(tzer, 2),
            nmea_tokenizer_get(tzer, 3),
            &gga.lat
        );
        
        getLatLon( 
            nmea_tokenizer_get(tzer, 4),
            nmea_tokenizer_get(tzer, 5),
            &gga.lon
        );
        
        TKN_INT( tzer, 6, gga.calcMode );
        TKN_INT( tzer, 7, gga.satCount );

        getFloat( nmea_tokenizer_get(tzer, 8), &gga.hdop );
        getDouble( nmea_tokenizer_get(tzer, 9), &gga.altitude );
        // units (M) 10
        getFloat( nmea_tokenizer_get(tzer, 11), &gga.geoidSep );
        // units (M) 12
        
        msg->obj = COPY( gga );

    } else if ( !memcmp(tok.p, "GLL", 3) ) {
        NmeaGeoGLL gll;
        
        msg->id = NMEA_GEO_GLL;
        
        getLatLon( 
            nmea_tokenizer_get(tzer, 1),
            nmea_tokenizer_get(tzer, 2),
            &gll.lat
        );
        
        getLatLon( 
            nmea_tokenizer_get(tzer, 3),
            nmea_tokenizer_get(tzer, 4),
            &gll.lon
        );
        
        getTime( nmea_tokenizer_get(tzer, 5), &gll.time );
        
        if (FAILED( getChar( nmea_tokenizer_get(tzer, 6), &gll.valid ) )) {
            goto end;
        }
        
        if (FAILED( getChar( nmea_tokenizer_get(tzer, 7), &gll.fixMode ) )) {
            goto end;
        }
        
        msg->obj = COPY( gll );

    } else if ( !memcmp(tok.p, "GSA", 3) ) {
        NmeaGeoGSA gsa;
        UINT32 i, idx = 1;
        memset( &gsa, 0, sizeof(gsa) );
        
        msg->id = NMEA_GEO_GSA;
        
        // check for empty msg
        if (FAILED( getChar( nmea_tokenizer_get(tzer, idx++), &gsa.mode ) )) {
            goto end;
        }
        
        if (FAILED( TKN_INT( tzer, idx++, gsa.flag ) )) {
            goto end;
        }
        
        for (i=0; i<GPS_MAX_SVS; i++ ) {
            if ( idx >= ( tzer->count - 3 ) ) break;
            TKN_INT( tzer, idx++, gsa.satUsed[i] );
        }
        
        getFloat( nmea_tokenizer_get(tzer, idx++), &gsa.pdop );
        getFloat( nmea_tokenizer_get(tzer, idx++), &gsa.hdop );
        getFloat( nmea_tokenizer_get(tzer, idx++), &gsa.vdop );
        
        msg->obj = COPY( gsa );

    } else if ( !memcmp(tok.p, "GSV", 3) ) {
        NmeaGeoGSV gsv;
        UINT32 i, idx = 1;
        memset( &gsv, 0, sizeof(gsv) );
        
        msg->id = NMEA_GEO_GSV;
        
        TKN_INT( tzer, idx++, gsv.msgCount );
        TKN_INT( tzer, idx++, gsv.msgIndex );
        TKN_INT( tzer, idx++, gsv.satInView );
        
        for (i=0; i<tzer->count; i++ ) {
            if ( idx >= tzer->count ) break;
            
            TKN_INT( tzer, idx++, gsv.satInfo[i].prn );
            TKN_INT( tzer, idx++, gsv.satInfo[i].elevation );
            TKN_INT( tzer, idx++, gsv.satInfo[i].azimuth );
            TKN_INT( tzer, idx++, gsv.satInfo[i].snr );
        }
        
        msg->obj = COPY( gsv );
    
    } else if ( !memcmp(tok.p, "MSS", 3) ) {
        NmeaGeoMSS mss;
        
        msg->id = NMEA_GEO_MSS;
        
        TKN_INT( tzer, 1, mss.strength );
        TKN_INT( tzer, 2, mss.snr );
        getFloat( nmea_tokenizer_get(tzer, 3), &mss.freq );
        TKN_INT( tzer, 4, mss.bps );
        TKN_INT( tzer, 5, mss.channel );
        
        msg->obj = COPY( mss );
    
    } else if ( !memcmp(tok.p, "RMC", 3) ) {
        NmeaGeoRMC rmc;
        
        msg->id = NMEA_GEO_RMC;
        
        // check for empty msg
        if (FAILED( getTime( nmea_tokenizer_get(tzer, 1), &rmc.time ) )) {
            goto end;
        }
        
        getChar( nmea_tokenizer_get(tzer, 2), &rmc.valid );
        
        getLatLon( 
            nmea_tokenizer_get(tzer, 3),
            nmea_tokenizer_get(tzer, 4),
            &rmc.lat
        );
        
        getLatLon( 
            nmea_tokenizer_get(tzer, 5),
            nmea_tokenizer_get(tzer, 6),
            &rmc.lon
        );
        
        getFloat( nmea_tokenizer_get(tzer, 7), &rmc.speed );
        getFloat( nmea_tokenizer_get(tzer, 8), &rmc.bearing );
        getDate( nmea_tokenizer_get(tzer, 9), &rmc.date );
        // magVariation (not supported by CSR) 10
        // magDirection (not supported by CSR) 11
        getChar( nmea_tokenizer_get(tzer, 12), &rmc.fixMode );
        
        msg->obj = COPY( rmc );

    } else if ( !memcmp(tok.p, "VTG", 3) ) {
        NmeaGeoVTG vtg;
        
        msg->id = NMEA_GEO_VTG;
        
        getFloat( nmea_tokenizer_get(tzer, 1), &vtg.bearingTrue );
        // reference (T) 2
        getFloat( nmea_tokenizer_get(tzer, 3), &vtg.bearingMagnetic );
        // reference (M) 4
        getFloat( nmea_tokenizer_get(tzer, 5), &vtg.speedKnots );
        // units (N) 6
        getFloat( nmea_tokenizer_get(tzer, 7), &vtg.speedKm );
        // units (K) 8
        getChar( nmea_tokenizer_get(tzer, 9), &vtg.fixMode );
        
        msg->obj = COPY( vtg );
    
    } else if ( !memcmp(tok.p, "ZDA", 3) ) {
        NmeaGeoZDA zda;
    
        msg->id = NMEA_GEO_ZDA;
        
        getTime( nmea_tokenizer_get(tzer, 1), &(zda.time) );
        TKN_INT( tzer, 2, zda.date.day );
        TKN_INT( tzer, 3, zda.date.month );
        TKN_INT( tzer, 4, zda.date.year );
        // Local zone hour (not supported by CSR) 5
        // Local zone minutes (not supported by CSR) 6
        
        msg->obj = COPY( zda );
        
    } else {
        WARN("unknown geo message: '%.*s'", tok.length, tok.p);
        nmea_tokenizer_print( tzer );
        return EXIT_FAILURE;
    }

end:
    DEBUG_MSG_IO("0x%02x/0x%04x", msg->type, msg->id);
    return EXIT_SUCCESS;
}

EXIT_CODE nmea_msg_read( Message* msg, Buffer* buff ) 
{
    Token tok;
    NmeaTokenizer tzer;
    
    nmea_tokenizer_init(&tzer, (char*)buff->data, (char*)(buff->data + buff->len));
    tok = nmea_tokenizer_get(&tzer, 0);
    
    // Geographic NMEA message
    if ( !memcmp(tok.p, NMEA_GEO_MSG_PREFIX, strlen(NMEA_GEO_MSG_PREFIX)) ) {
        return nmea_msg_read_geo( msg, &tzer );
    }
    
    // Sirf custom message
    else if ( !memcmp(tok.p, NMEA_SIRF_MSG_PREFIX, strlen(NMEA_SIRF_MSG_PREFIX)) ) {
        return nmea_msg_read_sirf( msg, &tzer );
    }
    
    WARN("Unknown message prefix: '%.*s'", tok.length, tok.p);
    nmea_tokenizer_print( &tzer );
    return EXIT_FAILURE;
}
