/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _GPS_STATE_H_
#define _GPS_STATE_H_

#include "gps_comm.h"
#include "thread.h"


typedef UINT8 GpsStateFlag;
enum _GpsStateFlag {
    GPS_FLAG_HAS_PATCH
};

typedef UINT8 GpsProtocol;
enum _GpsProtocol {
    GPS_PROTO_OSP               = 0,
    GPS_PROTO_NMEA              = 1
};

typedef UINT8 GpsDispatchCode;
enum _GpsDispatchCode {
    GPS_DISPATCH_STATUS,
    GPS_DISPATCH_LOCATION,
    GPS_DISPATCH_SV_STATUS,
    GPS_DISPATCH_MAX,
};


typedef struct {
    UINT8 ggaRate;
    UINT8 gllRate;
    UINT8 gsaRate;
    UINT8 gsvRate;
    UINT8 rmcRate;
    UINT8 vtgRate;
    UINT8 mssRate;
    UINT8 epeRate;
    UINT8 zdaRate;
} GpsGeoRates;

typedef struct {
    GpsComm                 comm;
    GpsCallbacks            callbacks;
    void*                   internal;
} GpsState;


GpsState* gps_state_init( GpsCallbacks* callbacks );
void gps_state_free( GpsState* s );

int gps_state_get_flag( GpsState* s, GpsStateFlag flag );
void gps_state_set_flag( GpsState* s, GpsStateFlag flag, int set );

void gps_state_get_geo_rates( GpsState* s, GpsGeoRates* rates );

GpsProtocol gps_state_get_protocol( GpsState* s );
void gps_state_set_protocol( GpsState* s, GpsProtocol p );

GpsStatusValue gps_state_get_status( GpsState* s );
void gps_state_set_status( GpsState* s, GpsStatusValue status );

void gps_state_get_location( GpsState* s, GpsLocation* loc );
void gps_state_set_location( GpsState* s, GpsLocation* loc );

int gps_state_get_sat_used( GpsState* s, UINT8 prn );
void gps_state_set_sat_used( GpsState* s, UINT8* prns, int len );
void gps_state_clear_sat_used( GpsState* s );

void gps_state_get_sv_status( GpsState* s, GpsSvStatus* status );
int gps_state_add_sv_status( GpsState* s, GpsSvInfo* info );
void gps_state_clear_sv_status( GpsState* s );

Thread* gps_state_create_thread( GpsState* s, const char* name, void (*start)(void *), void* arg);
void gps_state_dispatch( GpsState* s, GpsDispatchCode code );

void gps_state_acquire_wakelock( GpsState* s );
void gps_state_release_wakelock( GpsState* s );

#endif /* _GPS_STATE_H_ */
