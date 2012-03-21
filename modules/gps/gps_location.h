/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _GPS_LOCATION_H_
#define _GPS_LOCATION_H_

#include "gps_datetime.h"

void gps_location_set_altitude( GpsLocation* location, double altitude );
void gps_location_set_bearing( GpsLocation* location, float bearing );
void gps_location_set_datetime( GpsLocation* location, GpsDate* d, GpsTime* t );
void gps_location_set_latlon( GpsLocation* location, double lat, double lon );
void gps_location_set_speed( GpsLocation* location, float speed );
void gps_location_set_time( GpsLocation* location, GpsTime* t );

#endif /* _GPS_LOCATION_H_ */
