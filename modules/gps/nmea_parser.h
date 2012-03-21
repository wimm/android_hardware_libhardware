/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _NMEA_PARSER_H_
#define _NMEA_PARSER_H_

#include "message_queue.h"
#include "nmea_types.h"

typedef struct {
    UINT32  pos;
    UINT32  overflow;
    UINT8   in[ NMEA_MAX_SIZE+1 ];
    
    MessageQueue*       msgQueue;
    GpsCallbacks*       callbacks;
} NmeaParser;

NmeaParser* nmea_parser_init( 
    MessageQueue* msgQueue,
    GpsCallbacks* callbacks 
);

void nmea_parser_free( NmeaParser* p );

void nmea_parser_read( NmeaParser* p, UINT8* buff, UINT32 len );

#endif /* _NMEA_PARSER_H_ */
