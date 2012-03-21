/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */


#include "gps_location.h"

void _gps_location_set_datetime( GpsLocation* location, struct tm* tm, int ms ) 
{
    long seconds, utcOffset;
    struct tm tm_local, tm_utc;
    time_t now = time(NULL);

    gmtime_r( &now, &tm_utc );
    localtime_r( &now, &tm_local );

    utcOffset = mktime( &tm_utc ) - mktime( &tm_local );
    seconds = mktime( tm ) + utcOffset;

    // DEBUG("utcOffset=%ld seconds=%ld", utcOffset, seconds);

    location->timestamp = ( seconds * 1000 ) + ms;
}

void gps_location_set_datetime( GpsLocation* location, GpsDate* d, GpsTime* t ) 
{
    struct tm tm;
    
    if ( d->year == 0 ) {
        WARN("invalid");
        return;
    }
    
    if (( t->hours == 0 ) &&
        ( t->minutes == 0 ) &&
        ( t->seconds == 0 ) &&
        ( t->ms == 0 ) ) {
        WARN("invalid");
        return;
    }
    
    tm.tm_hour  = t->hours;
    tm.tm_min   = t->minutes;
    tm.tm_sec   = t->seconds;
    tm.tm_year  = d->year - 1900;
    tm.tm_mon   = d->month - 1;
    tm.tm_mday  = d->day;
    tm.tm_isdst = -1;
    
    _gps_location_set_datetime( location, &tm, t->ms );
}

void gps_location_set_time( GpsLocation* location, GpsTime* t ) 
{
    GpsDate date;
    struct tm tm;
    time_t now = time(NULL);
    
    if (( t->hours == 0 ) &&
        ( t->minutes == 0 ) &&
        ( t->seconds == 0 ) &&
        ( t->ms == 0 ) ) {
        WARN("invalid");
        return;
    }
    
    gmtime_r( &now, &tm );
    tm.tm_hour  = t->hours;
    tm.tm_min   = t->minutes;
    tm.tm_sec   = t->seconds;
    
    _gps_location_set_datetime( location, &tm, t->ms );
}

void gps_location_set_latlon( GpsLocation* location, double lat, double lon )
{
    location->flags |= GPS_LOCATION_HAS_LAT_LONG;
    location->latitude = lat;
    location->longitude = lon;
}

void gps_location_set_altitude( GpsLocation* location, double altitude )
{
    location->flags |= GPS_LOCATION_HAS_ALTITUDE;
    location->altitude = altitude;
}

void gps_location_set_speed( GpsLocation* location, float speed )
{
    location->flags |= GPS_LOCATION_HAS_SPEED;
    location->speed = speed;
}

void gps_location_set_bearing( GpsLocation* location, float bearing )
{
    location->flags |= GPS_LOCATION_HAS_BEARING;
    location->bearing = bearing;
}
