/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * NMEA parsing
 *
 */

#include "nmea_parser.h"
#include "nmea_msg.h"

EXIT_CODE _nmea_parser_parse( NmeaParser* p, UINT8* buff, UINT32 len )
{
    Buffer b;
    Message *msg = message_init();

    b = (Buffer) { buff, len, 0 };
    if (FAILED( nmea_msg_read( msg, &b ) )) {
        message_release( msg );
        return EXIT_FAILURE;
    }

    message_queue_add( p->msgQueue, msg );
    message_release( msg );
    
    /* commented out as this appears to leak memory
    // dispatch raw NMEA
    if (p->callbacks && p->callbacks->nmea_cb) {
        p->callbacks->nmea_cb(NOW(), (char*)buff, len);
    }
    */
    return EXIT_SUCCESS;
}

EXIT_CODE _nmea_parser_validate( NmeaParser* p, UINT8* buff, UINT32 len )
{
    // check for valid length
    if (len < 10) {
        WARN("message too short '%.*s'", len, buff);
        return EXIT_FAILURE;
    }
    
    // TODO 
    // calc checksum
    
    return EXIT_SUCCESS;
}

void nmea_parser_read( NmeaParser* p, UINT8* buff, UINT32 len )
{
    UINT32 nn;
    for (nn = 0; nn < len; nn++) {
        // DEBUG("%d = 0x%x", nn, buff[nn]);
        
        // clear on null char
        if ( buff[nn] == '\0' ) {
            DEBUG("ignoring null");
            p->overflow = 0;
            p->pos = 0;
            continue;
        }
        
        // ignore input once we've overflowed, 
        // until we get a newline
        if (p->overflow) {
            if ( buff[nn] == '\n' )
                p->overflow = 0;
            continue;
        }

        // check for overflow
        if (p->pos >= sizeof(p->in)-1 ) {
            ERROR("buffer overflow");
            p->overflow = 1;
            p->pos = 0;
            continue;
        }

        // ignore new input until we get a $
        if ( ( p->pos == 0 ) && ( buff[nn] != '$' ) ) {
            WARN("dropping 0x%x '%c'", buff[nn], buff[nn]);
            continue;
        }

        if ( buff[nn] == '\r' ) { 
            /* ignore */ 
        } else if (buff[nn] == '\n') {
#if DBG_NMEA
            DEBUG("'%.*s'", p->pos, p->in);
#endif
            if (OK( _nmea_parser_validate( p, p->in, p->pos ) ))
                _nmea_parser_parse( p, p->in, p->pos );
            p->pos = 0;
        } else {
            p->in[p->pos] = buff[nn];
            p->pos++;
        }
    }
}

NmeaParser* nmea_parser_init( 
    MessageQueue* msgQueue,
    GpsCallbacks* callbacks 
) {
    NmeaParser* p = (NmeaParser*) calloc( 1, sizeof( NmeaParser ) );
    
    p->msgQueue = msgQueue;
    p->callbacks = callbacks;
    
    return p;
}

void nmea_parser_free( NmeaParser* p )
{
    if ( !p ) return;
    free( p );
}

