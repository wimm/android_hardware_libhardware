/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * NMEA GP message handler functions
 *
 */

#include "work_thread_geo.h"

#include "nmea_types.h"
#include "gps_location.h"

#if DBG_GEO
#define DEBUG_GEO(...)                  DEBUG(__VA_ARGS__)
#else
#define DEBUG_GEO(...)                  ((void)0)
#endif /* DBG_GEO */

static void work_thread_geo_handle_gga( GpsState* s, NmeaGeoGGA* data )
{
    GpsLocation location;
    location.size = sizeof(GpsLocation);
    
    DEBUG_GEO("time=%02d:%02d:%02d.%03d", 
        data->time.hours, 
        data->time.minutes, 
        data->time.seconds, 
        data->time.ms
    );
    
    if ( ( data->lat == 0.0f ) && ( data->lon == 0.0f ) ) 
        return;
    
    if ( data->calcMode == NMEA_GEO_CALC_MODE_INVALID ) {
        return;
    }
    
    DEBUG_GEO("lat=%f lon=%f calcMode=%d satCount=%d hdop=%f altitude=%f geoidSep=%f", 
        data->lat,
        data->lon,
        data->calcMode,
        data->satCount,
        data->hdop,
        data->altitude,
        data->geoidSep
    );

    gps_location_set_time( &location, &data->time );
    gps_location_set_latlon( &location, data->lat, data->lon );
    gps_location_set_altitude( &location, data->altitude );

    gps_state_set_location( s, &location );
    gps_state_dispatch( s, GPS_DISPATCH_LOCATION );
}
            
static void work_thread_geo_handle_gll( GpsState* s, NmeaGeoGLL* data )
{
    GpsLocation location;
    location.size = sizeof(GpsLocation);
    
    if ( data->valid == NMEA_GEO_FLAG_INVALID ) {
        return;
    }
    
    if ( data->fixMode == NMEA_GEO_FIX_MODE_INVALID ) {
        return;
    }
    
    DEBUG_GEO("lat=%f lon=%f time=%02d:%02d:%02d.%04d valid=%c fixMode=%c", 
        data->lat,
        data->lon,
        data->time.hours, 
        data->time.minutes, 
        data->time.seconds, 
        data->time.ms * 1000, 
        data->valid,
        data->fixMode
    );
        
    gps_location_set_time( &location, &data->time );
    gps_location_set_latlon( &location, data->lat, data->lon );

    gps_state_set_location( s, &location );
    gps_state_dispatch( s, GPS_DISPATCH_LOCATION );
}
            
static void work_thread_geo_handle_gsa( GpsState* s, NmeaGeoGSA* data )
{
    if ( data->flag == NMEA_GEO_PRECISION_FLAG_NO_FIX ) {
        gps_state_clear_sat_used( s );
        return;
    }
    
#if DBG_GEO
    {
        int _debug_i, _debug_o = 0, _debug_p;
        char _debug_satUsedStr[1024];
        memset( _debug_satUsedStr, 0, 1024 );
        
        for (_debug_i=0; _debug_i<GPS_MAX_SVS; _debug_i++) {
            _debug_p = data->satUsed[_debug_i];
            if ( _debug_p == 0 ) break;
            
            _debug_o += sprintf( 
                _debug_satUsedStr + _debug_o, 
                ( ( _debug_o == 0 ) ? "%d" : ",%d" ), 
                _debug_p
            );
        }
        
        DEBUG_GEO("mode=%c flag=%d satUsed=%s", 
            data->mode, 
            data->flag,
            _debug_satUsedStr
        );
    }
#endif
    
    gps_state_set_sat_used( s, data->satUsed, GPS_MAX_SVS );
}

static void work_thread_geo_handle_gsv( GpsState* s, NmeaGeoGSV* data )
{
    int i, svCount;
    GpsSvInfo svInfo;
    NmeaGeoSatInfo* satInfo;
    
    DEBUG_GEO("msgCount=%d msgIndex=%d satInView=%d", 
        data->msgCount, 
        data->msgIndex, 
        data->satInView
    );
    
    // first message
    if ( data->msgIndex == 1 ) {
        svCount = 0;
        gps_state_clear_sv_status( s );
    }
    
    // copy sat info
    for (i=0; i<GPS_MAX_SVS; i++) {
        satInfo = &data->satInfo[i];
        
        // no more sats in this msg
        if ( satInfo->prn == 0 ) break;
        
        DEBUG_GEO("prn=%02d snr=%02d elevation=%02d azimuth=%03d", 
            satInfo->prn, 
            satInfo->snr, 
            satInfo->elevation, 
            satInfo->azimuth
        );
        
        // copy info
        svInfo.prn = satInfo->prn;
        svInfo.snr = satInfo->snr;
        svInfo.elevation = satInfo->elevation;
        svInfo.azimuth = satInfo->azimuth;
        
        svCount = gps_state_add_sv_status( s, &svInfo );
    }
    
    // last message
    if ( data->msgIndex == data->msgCount ) {
        if ( data->satInView != svCount ) {
            WARN("invalid sat count (%d!=%d)", 
                data->satInView, svCount);
        } else {
            gps_state_dispatch( s, GPS_DISPATCH_SV_STATUS );
        }
    }
}

static void work_thread_geo_handle_mss( GpsState* s, NmeaGeoMSS* data )
{
    ERROR("TODO");
}
            
static void work_thread_geo_handle_rmc( GpsState* s, NmeaGeoRMC* data )
{
    GpsLocation location;
    location.size = sizeof(GpsLocation);
    
    if ( data->valid == NMEA_GEO_FLAG_INVALID ) {
        return;
    }
    
    if ( data->fixMode == NMEA_GEO_FIX_MODE_INVALID ) {
        return;
    }
    
    DEBUG_GEO("time=%02d:%02d:%02d.%04d valid=%c lat=%f lon=%f speed=%f bearing=%f date=%02d/%02d/%02d fixMode=%c", 
        data->time.hours, 
        data->time.minutes, 
        data->time.seconds, 
        data->time.ms * 1000, 
        data->valid,
        data->lat,
        data->lon,
        data->speed,
        data->bearing,
        data->date.day,
        data->date.month,
        data->date.year,
        data->fixMode
    );
    
    gps_location_set_datetime( &location, &data->date, &data->time );
    gps_location_set_latlon( &location, data->lat, data->lon );
    gps_location_set_speed( &location, data->speed );
    gps_location_set_bearing( &location, data->bearing );

    gps_state_set_location( s, &location );
    gps_state_dispatch( s, GPS_DISPATCH_LOCATION );
}
            
static void work_thread_geo_handle_vtg( GpsState* s, NmeaGeoVTG* data )
{
    ERROR("TODO");
}
            
static void work_thread_geo_handle_zda( GpsState* s, NmeaGeoZDA* data )
{
    ERROR("TODO");
}

void work_thread_geo_handle_msg( WorkThread* t, Message *msg )
{
    GpsState* s = t->workState.gpsState;
    DEBUG_GEO("0x%x", msg->id);
    
    // ignore empty messages
    if ( msg->obj == NULL ) return;
    
    switch (msg->id) {
        case NMEA_GEO_GGA:
            work_thread_geo_handle_gga(s, msg->obj);
            break;
            
        case NMEA_GEO_GLL:
            work_thread_geo_handle_gll(s, msg->obj);
            break;
            
        case NMEA_GEO_GSA:
            work_thread_geo_handle_gsa(s, msg->obj);
            break;

        case NMEA_GEO_GSV:
            work_thread_geo_handle_gsv(s, msg->obj);
            break;
            
        case NMEA_GEO_MSS:
            work_thread_geo_handle_mss(s, msg->obj);
            break;
            
        case NMEA_GEO_RMC:
            work_thread_geo_handle_rmc(s, msg->obj);
            break;
            
        case NMEA_GEO_VTG:
            work_thread_geo_handle_vtg(s, msg->obj);
            break;
            
        case NMEA_GEO_ZDA:
            work_thread_geo_handle_zda(s, msg->obj);
            break;
            
        default:
            WARN("unhandled GEO message: 0x%x", msg->id);
            break;
    }
}

