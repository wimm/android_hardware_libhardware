/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * NMEA tokenizer
 *
 */

#include "nmea_tokenizer.h"

EXIT_CODE getInt( Token t, UINT8 base, UINT32* result ) 
{
    char* c;
    char temp[16];

    if (t.length == 0 ) {
        return EXIT_FAILURE;
    }
    
    if (t.length >= sizeof(temp)) {
        WARN("too large: '%.*s'", t.length, t.p);
        return EXIT_FAILURE;
    }

    memcpy( temp, t.p, t.length );
    temp[t.length] = '\0';
    
    *result = strtoul( temp, &c, base );
    if (*c != '\0') {
        WARN("invalid: '%s'", temp);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

EXIT_CODE getDouble( Token t, double* result ) 
{
    char  *c;
    char  temp[16];

    if (t.length == 0 ) {
        return EXIT_FAILURE;
    }
    
    if (t.length >= sizeof(temp)) {
        WARN("too large: '%.*s'", t.length, t.p);
        return EXIT_FAILURE;
    }

    memcpy( temp, t.p, t.length );
    temp[t.length] = '\0';
    
    *result = strtod( temp, &c );
    if (*c != '\0') {
        WARN("invalid: '%s'", temp);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

EXIT_CODE getFloat( Token t, float* result ) 
{
    double d;
    if (FAILED( getDouble( t, &d ) ))
        return EXIT_FAILURE;
        
    *result = d;
    return EXIT_SUCCESS;
}

EXIT_CODE getChar( Token t, char* result ) 
{
    if (t.length == 0 ) {
        return EXIT_FAILURE;
    }
    
    if (t.length > 1) {
        WARN("too large: '%.*s'", t.length, t.p);
        return EXIT_FAILURE;
    }
    
    *result = t.p[0];
    return EXIT_SUCCESS;
}

// format: ddmmyy
EXIT_CODE getDate( Token t, GpsDate* date ) {
    char temp[7];
    int count;
    int day, month, year;
    
    if ( t.length != 6 ) {
        if ( t.length > 0 )
            WARN("invalid length '%.*s'", t.length, t.p);
        return EXIT_FAILURE;
    }
    
    strncpy( temp, t.p, t.length );
    
    count = sscanf(temp, 
        "%02d%02d%02d", 
        &day, &month, &year
    );
    
    if (count == 3) {
        date->day = day;
        date->month = month;
        date->year = year + 2000;
        return EXIT_SUCCESS;
    }
    
    WARN("invalid: '%.*s'", t.length, t.p);
    return EXIT_FAILURE;
}

// format: hhmmss OR hhmmss.sss
EXIT_CODE getTime( Token t, GpsTime* time ) {
    char temp[11];
    int count, unused;
    int hours, minutes, seconds;
    
    // don't error for '0'
    if (( t.length == 1 ) && (t.p[0] == '0')) {
        return EXIT_FAILURE;
    }
    
    if (( t.length < 6 ) || ( t.length > 10 )) {
        if ( t.length > 0 )
            WARN("invalid length '%.*s'", t.length, t.p);
        return EXIT_FAILURE;
    }
    
    strncpy( temp, t.p, t.length );
    
    count = sscanf(temp, 
        "%02d%02d%02d.%d", 
        &hours, &minutes, &seconds, &unused
    );
    
    time->hours = hours;
    time->minutes = minutes;
    time->seconds = seconds;
    
    if (count == 3) {
        return EXIT_SUCCESS;
    }
    
    if (count == 4) {
        float f;
        if (OK( getFloat(t, &f) )) {
            f -= floor(f);
            time->ms = f * 1000;
        }
        return EXIT_SUCCESS;
    }
    
    WARN("invalid: '%.*s'", t.length, t.p);
    return EXIT_FAILURE;
}

EXIT_CODE getLatLon( Token tV, Token tI, double* result ) {
    int degrees;
    double minutes;
    
    double val;
    char indicator = '\0';
    
    if (FAILED( getDouble( tV, &val ) )) 
        return EXIT_FAILURE;
        
    getChar( tI, &indicator );
    
    degrees = floor( val / 100.0f );
    minutes = val - ( degrees * 100.0f );
    *result = degrees + ( minutes / 60.0f );
    
    if ( (indicator == 'S') || (indicator == 'W') ) {
        *result *= -1;
    }
    
    return EXIT_SUCCESS;
}


Token nmea_tokenizer_get( NmeaTokenizer* t, UINT8 idx )
{
    Token  tok;
    static const char*  dummy = "";

    if (idx >= t->count || idx >= NMEA_MAX_TOKENS ) {
//    DEBUG("%d: out of range", idx);
        tok.p = dummy;
        tok.length = 0;
    } else {
        tok = t->tokens[idx];
//    DEBUG("%d: '%.*s'", idx, tok.end-tok.p, tok.p);
    }
    
    return tok;
}

void nmea_tokenizer_put( NmeaTokenizer* t, const char* p, UINT32 len )
{
    if (t->count < NMEA_MAX_TOKENS) {
        t->tokens[t->count].p = p;
        t->tokens[t->count].length = len;
        t->count++;
    } else {
        ERROR("token overflow");
        nmea_tokenizer_print( t );
    }
}

UINT32 nmea_tokenizer_init( NmeaTokenizer* t, const char* p, const char* end )
{
    char*  q;
    
    memset( t, 0, sizeof( *t ) );
    
    // TODO: check checksum!
    
    // discard $
    if ( *p == '$' ) p++;

    // get rid of checksum at the end of the sentence
    if (end >= p+3 && end[-3] == '*') {
        end -= 3;
    }
    
    t->count = 0;

    while (p < end) {
        const char* q;
        
        q = memchr(p, ',', end-p);
        if (q == NULL) q = end;

        nmea_tokenizer_put( t, p, q-p );
        
        p = q+1;
    }
    
    if ( end[-1] == ',' )
        nmea_tokenizer_put( t, end-1, 0 );

    
#if 0 // GPS_DEBUG
    {
        UINT32  n;
        DEBUG("Found %d tokens", t->count);
        for (n = 0; n < t->count; n++) {
            Token  tok = nmea_tokenizer_get(t,n);
            DEBUG("%2d: '%.*s'", n, tok.length, tok.p);
        }
    }
#endif

    return t->count;
}

EXIT_CODE nmea_tokenizer_get_data( NmeaTokenizer* tzer, UINT32 start, UINT8* data, UINT32 length )
{
    UINT32 i = 0, idx, byte = 0;    
    if ( length == 0 ) return EXIT_SUCCESS;
                
    // validate length
    if ((start+length) > tzer->count) {
        ERROR("not enough data in NMEA for (%d+%d) > %d", start, length, tzer->count);
        return EXIT_FAILURE;
    }
    
    // read bytes
    for (idx = start; idx < length; idx++) {
        if (FAILED( TKN_HEX( tzer, idx, byte ) )) {
            ERROR("invalid hex value");
            return EXIT_FAILURE;
        }
        
        if ( byte > 255 ) {
            ERROR("value too large for byte: 0x%x", byte);
            return EXIT_FAILURE;
        }
        
        data[i++] = (byte&0xFF);
    }
    
    return EXIT_SUCCESS;
}

void nmea_tokenizer_print( NmeaTokenizer* t ) 
{
    if ( t->count == 0 ) return;
    
    Token* first = &t->tokens[0];
    Token* last = &t->tokens[t->count -1];
    
    const char *start = first->p;
    const char *end = last->p + last->length;
    
    INFO("%.*s", end - start, start);
}
