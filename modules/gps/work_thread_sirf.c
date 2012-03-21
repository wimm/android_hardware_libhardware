/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * NMEA message handler functions
 *
 */

#include "work_thread_sirf.h"
#include "work_thread_eclm.h"

#include "osp_types.h"
#include "patch_manager.h"


EXIT_CODE sirf_set_comm_osp_to_nmea(
    WorkState* s, 
    GpsBaudRate baud
) {
    Message *msg;
    OspSetNmeaMode obj;
    GpsGeoRates geoRates;
    
    // baud rate of 115200 can not fit into UINT16
    if ( baud > 0xFFFF ) {
        ERROR("baud rate %d unsupported by message", baud);
        return EXIT_FAILURE;
    }
    
    gps_state_get_geo_rates( s->gpsState, &geoRates );
    
    memset( &obj, 0, sizeof(obj) );
    obj.debugMode   = OSP_NMEA_DEBUG_NO_CHANGE;
    obj.ggaRate     = geoRates.ggaRate;
    obj.gllRate     = geoRates.gllRate;
    obj.gsaRate     = geoRates.gsaRate;
    obj.gsvRate     = geoRates.gsvRate;
    obj.rmcRate     = geoRates.rmcRate;
    obj.vtgRate     = geoRates.vtgRate;
    obj.mssRate     = geoRates.mssRate;
    obj.epeRate     = geoRates.epeRate;
    obj.zdaRate     = geoRates.zdaRate;
    obj.baudRate    = baud;
    
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_SET_NMEA_MODE;
    msg->dispose = osp_msg_dispose;
    msg->obj = COPY( obj );
    
    message_queue_add( s->writeQueue, msg );
    message_wait_until_sent( msg, OSP_TIMEOUT_MS );
    
    // sometimes does not take effect until second message
    usleep(100*1000);

    message_queue_add( s->writeQueue, msg );
    message_wait_until_sent( msg, OSP_TIMEOUT_MS );
    
    message_release( msg );
    return EXIT_SUCCESS;
}

EXIT_CODE sirf_set_comm_osp(
    WorkState* s, 
    GpsBaudRate baud
) {
    Message *msg;
    OspSerialConfig obj;
    
    obj.bitRate = baud;
    
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_SET_SERIAL_PORT;
    msg->dispose = osp_msg_dispose;
    msg->obj = COPY( obj );
    
    message_queue_add( s->writeQueue, msg );
    message_wait_until_sent( msg, OSP_TIMEOUT_MS );
    message_release( msg );
    
    return EXIT_SUCCESS;
}

EXIT_CODE sirf_set_comm_nmea(
    WorkState* s, 
    GpsProtocol protocol,
    GpsBaudRate baud
) {
    Message *msg;
    SirfSerialConfig obj;
    GpsProtocol previousProtocol = gps_state_get_protocol( s->gpsState );
    
    obj.protocol = protocol;
    obj.bitRate = baud;
    
    msg = message_init();
    msg->type = MSG_TYPE_SIRF;
    msg->id = SIRF_I_SET_SERIAL_PORT;
    msg->obj = COPY( obj );
    
    message_queue_add( s->writeQueue, msg );
    message_wait_until_sent( msg, OSP_TIMEOUT_MS );
    
    // if we don't sleep after sending the serial cfg message, all incoming
    // data turns to nulls
    usleep(100*1000);
    
    message_release( msg );
    return EXIT_SUCCESS;
}

EXIT_CODE _sirf_set_comm( WorkState* s, GpsProtocol protocol, GpsBaudRate rate )
{
    GpsComm *comm = &s->gpsState->comm;

    GpsProtocol previousProtocol = gps_state_get_protocol( s->gpsState );
    GpsBaudRate previousRate = comm->rate;
    
    INFO("protocol=%d rate=%d", protocol, rate);
    
    if ( ( previousProtocol == protocol ) && ( previousRate == rate ) ) {
        DEBUG("unchanged");
        return EXIT_SUCCESS;
    }

    if ( previousProtocol == GPS_PROTO_OSP ) {
        if ( protocol == GPS_PROTO_NMEA ) {
            FAIL_UNLESS( sirf_set_comm_osp_to_nmea( s, rate ) );
        } else {
            FAIL_UNLESS( sirf_set_comm_osp( s, rate ) );
        }
    } else {
        FAIL_UNLESS( sirf_set_comm_nmea( s, protocol, rate ) );
    }
    
    gps_comm_cfg( comm, rate );
    gps_state_set_protocol( s->gpsState, protocol );
    
    // wait for valid message to be received
    if (FAILED( message_queue_wait_for_add( s->readQueue, GPS_WAIT_FOR_MSG_DELAY ) )) {
        ERROR("failed to set protocol=%d rate=%d", protocol, rate);
        gps_comm_cfg( comm, previousRate );
        gps_state_set_protocol( s->gpsState, previousProtocol );
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

EXIT_CODE sirf_set_comm( WorkState* s, GpsProtocol protocol, GpsBaudRate rate )
{
    int attempt;
    for ( attempt=0; attempt<3; attempt++ ) {
        if (OK( _sirf_set_comm( s, protocol, rate ) )) 
            return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}


EXIT_CODE sirf_init_nav( WorkState* s, SirfResetMode resetMode )
{
    Message *msg;
    SirfLlaNavInit obj;
    memset( &obj, 0, sizeof( obj ) );
    
    INFO("resetMode=%d", resetMode);
    
    if ( gps_state_get_protocol( s->gpsState ) != GPS_PROTO_NMEA ) {
        ERROR("OSP mode not supported");
        return EXIT_FAILURE;
    }

    obj.clockDrift = GPS_CLOCK_DRIFT;
    obj.channelCount = GPS_CHANNEL_MAX;
    obj.resetMode = resetMode;
    
    // only these modes take init data into consideration
    if ( ( resetMode == SIRF_RESET_HOT ) || 
         ( resetMode == SIRF_RESET_WARM_WITH_INIT ) )
    {
        gps_state_get_location( s->gpsState, &obj.location );
        get_gps_week_date_time( &obj.dateTime );
    }
    
    msg = message_init();
    msg->type = MSG_TYPE_SIRF;
    msg->id = SIRF_I_LLA_NAV_INIT;
    msg->obj = COPY( obj );
    
    message_queue_add( s->writeQueue, msg );
    message_wait_until_sent( msg, OSP_TIMEOUT_MS );
    
    // sometimes does not take effect until second message
    usleep(100*1000);

    message_queue_add( s->writeQueue, msg );
    message_wait_until_sent( msg, OSP_TIMEOUT_MS );
    
    message_release( msg );
    
    return EXIT_SUCCESS;
}

EXIT_CODE sirf_log_sw_version(
    WorkState* s
) {
    Message *msg;
    Message *replyMsg;
    OspSwVersion swVersion;
    
    // send Patch Manager Start Request
    msg = message_init();
    msg->type = MSG_TYPE_OSP;
    msg->id = OSP_ID_POLL_SW_VERSION;
    msg->dispose = osp_msg_dispose;
    message_queue_add( s->writeQueue, msg );
    message_release( msg );
    
    // wait for Patch Manager Prompt
    FAIL_UNLESS( message_queue_wait_for_message( 
        s->readQueue,
        OSP_TIMEOUT_MS,
        MSG_TYPE_OSP,
        OSP_ID_SW_VERSION,
        &replyMsg
    ));

    swVersion = *( (OspSwVersion*)(replyMsg->obj) );
    INFO("sirfVersion = %s", swVersion.sirfVersion);
    INFO("customerVersion = %s", swVersion.customerVersion);
    
    message_queue_remove( s->readQueue, replyMsg );
    return EXIT_SUCCESS;
}

EXIT_CODE sirf_poll_sgee_age( WorkState* s )
{
    int prn;
    Message* msg;
    EclmSgeeAgeResponse* data;
    
    for (prn=0; prn<32 && s->active; prn++) {
        if (OK( eclm_send_sgee_age_request( s, prn, NULL ) )) {
        
            if (OK( message_queue_wait_for_message( 
                s->readQueue,
                OSP_TIMEOUT_MS,
                MSG_TYPE_OSP,
                OSP_ID_SGEE_AGE_RESP,
                &msg
            ) )) {
                data = (EclmSgeeAgeResponse*)msg->obj;   
                INFO("prn=%d age=%d interval=%d", prn, data->age, data->interval);
                message_queue_remove( s->readQueue, msg );
            }
        }
    }
    
    return EXIT_SUCCESS;
}


EXIT_CODE sirf_patch_rom( WorkState* s )
{
    EXIT_CODE result = EXIT_FAILURE;
    
    GpsProtocol initialProto = gps_state_get_protocol( s->gpsState );
    GpsBaudRate initialRate = s->gpsState->comm.rate;
    INFO("initial protocol=%d rate=%d", initialProto, initialRate);
    
    // config comm
    FAIL_UNLESS( sirf_set_comm( s, GPS_PROTO_OSP, GPS_PATCH_RATE ) );

    sirf_log_sw_version( s );
    result = pm_send_patch_file( s, GPS_PATCH_PATH );
    sirf_log_sw_version( s );

    if ( result == EXIT_SUCCESS ) {
        gps_state_set_flag( s->gpsState, GPS_FLAG_HAS_PATCH, 1 );
    }
    
    // restore comm
    FAIL_UNLESS( sirf_set_comm( s, initialProto, initialRate ) );

    return result;
}

EXIT_CODE sirf_wait_for_ok_to_send(
    WorkState* s
) {
    OspOkToSend cts;
    Message* ctsMsg;
    
    FAIL_UNLESS( message_queue_wait_for_message( 
        s->readQueue,
        OSP_TIMEOUT_MS,
        MSG_TYPE_OSP,
        OSP_ID_OK_TO_SEND,
        &ctsMsg
    ));
    
    cts = *((OspOkToSend*)ctsMsg->obj);
    DEBUG("OkToSend: %d", cts.okToSend);
        
    message_queue_remove( s->readQueue, ctsMsg );
    return EXIT_SUCCESS;
}
