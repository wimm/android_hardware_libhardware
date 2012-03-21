/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _GPS_DATETIME_H_
#define _GPS_DATETIME_H_

#include "gps.h"
        
        
typedef struct {
    UINT8 hours;
    UINT8 minutes;
    UINT8 seconds;
    UINT16 ms;
} GpsTime;

typedef struct {
    UINT8 day;    // 1-31
    UINT8 month;  // 1-12
    UINT16 year;  // full year (2021)
} GpsDate;

typedef struct {
    UINT16 weekNumber;
    UINT32 timeOfWeek;
} GpsWeekDateTime;


void get_gps_week_date_time( GpsWeekDateTime* t );


#endif /* _GPS_DATETIME_H_ */
