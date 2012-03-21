/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#include "gps_datetime.h"

#define GPS_WEEK_TIME               (604800)


time_t get_gps_utc_time( void )
{
    // convert local time to UTC time
    time_t t = time(NULL);
    return mktime( gmtime( &t ) );
}

time_t get_gps_time_epoch( void )
{
    struct tm tm;
    
    memset(&tm, 0, sizeof(tm));
    tm.tm_year  = 80;   // 1980
    tm.tm_mon   = 0;    // January
    tm.tm_mday  = 6;    // 6th
    tm.tm_isdst = -1;   // Unknown DST
    
    return mktime(&tm);
}

time_t get_gps_leap_epoch1( void )
{
    struct tm tm;
    
    memset(&tm, 0, sizeof(tm));
    tm.tm_year  = 112;  // 2012
    tm.tm_mon   = 6;    // July
    tm.tm_mday  = 1;    // 1st
    tm.tm_isdst = -1;   // Unknown DST
    
    return mktime(&tm);
}

int get_gps_leap_seconds( void )
{
    time_t now = get_gps_utc_time();
    
    // leap seconds will go from 15 to 16 on July 1, 2012
    if ( difftime( now, get_gps_leap_epoch1() ) >= 0 )
        return 16;
        
    return 15;
}

void get_gps_week_date_time( GpsWeekDateTime* t )
{
    double diff = difftime( get_gps_utc_time(), get_gps_time_epoch() );
    diff += get_gps_leap_seconds();
    
    t->weekNumber = diff / GPS_WEEK_TIME;
    t->timeOfWeek = (int)diff % GPS_WEEK_TIME;
}
