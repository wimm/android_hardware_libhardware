/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _NMEA_TOKENIZER_H_
#define _NMEA_TOKENIZER_H_

#include "gps_datetime.h"

#define NMEA_MAX_TOKENS         32

typedef struct {
    const char* p;
    UINT32 length;
} Token;

typedef struct {
    UINT32  count;
    Token   tokens[ NMEA_MAX_TOKENS ];
} NmeaTokenizer;


#define _TKN_INT(__t,__i,__v,__b)    \
    ({                          \
        EXIT_CODE __r;          \
        UINT32 __x;             \
        __r = getInt(nmea_tokenizer_get( __t, __i), __b, &__x ); \
        if(OK(__r)) __v = __x;  \
        __r;                    \
    })

#define TKN_INT(__t,__i,__v)    _TKN_INT(__t,__i,__v,10)
#define TKN_HEX(__t,__i,__v)    _TKN_INT(__t,__i,__v,16)

EXIT_CODE getInt( Token t, UINT8 base, UINT32* result ) ;
EXIT_CODE getDouble( Token t, double* result ) ;
EXIT_CODE getFloat( Token t, float* result ) ;
EXIT_CODE getChar( Token t, char* result ) ;

EXIT_CODE getDate( Token t, GpsDate* date );
EXIT_CODE getTime( Token t, GpsTime* time );

EXIT_CODE getLatLon( Token tV, Token tI, double* result );

EXIT_CODE nmea_tokenizer_get_data( NmeaTokenizer* tzer, UINT32 start, UINT8* data, UINT32 length );

Token nmea_tokenizer_get( NmeaTokenizer* t, UINT8 idx );
void nmea_tokenizer_put( NmeaTokenizer* t, const char* p, UINT32 len );

UINT32 nmea_tokenizer_init( NmeaTokenizer* t, const char* p, const char* end );

void nmea_tokenizer_print( NmeaTokenizer* t );

#endif /* _NMEA_TOKENIZER_H_ */
