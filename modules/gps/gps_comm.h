/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _GPS_COMM_H_
#define _GPS_COMM_H_

#include "gps.h"
#include <termios.h>

typedef UINT32 GpsBaudRate;
enum _GpsBaudRate {
    GPS_BAUD_1200              = 1200,
    GPS_BAUD_2400              = 2400,
    GPS_BAUD_4800              = 4800,
    GPS_BAUD_9600              = 9600,
    GPS_BAUD_19200             = 19200,
    GPS_BAUD_38400             = 38400,
    GPS_BAUD_57600             = 57600,
    GPS_BAUD_115200            = 115200
};

typedef struct {
    GpsBaudRate             rate;
    FILE_DESC               fd;
} GpsComm;

EXIT_CODE gps_comm_open( GpsComm* c, const char* device );
EXIT_CODE gps_comm_close( GpsComm* c );

EXIT_CODE gps_comm_cfg( GpsComm* c, GpsBaudRate rate );

EXIT_CODE gps_comm_write( GpsComm* c, UINT8* buf, UINT16 len );

#endif /* _GPS_COMM_H_ */
