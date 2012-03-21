/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * OSP parsing
 *
 */

#include "osp_parser.h"
#include "osp_msg.h"

EXIT_CODE _osp_parser_parse( OspParser* p, UINT8* buff, UINT32 len )
{
    Buffer b;
    Message *msg = message_init();

    b = (Buffer) { buff, len, 0 };
    if (FAILED( osp_msg_read( msg, &b ) )) {
        message_release( msg );
        return EXIT_FAILURE;
    }

    message_queue_add( p->msgQueue, msg );
    message_release( msg );
    
    return EXIT_SUCCESS;
}

EXIT_CODE _osp_parser_validate( OspParser* p, UINT8* buff, UINT32 len )
{
    UINT32 i = 0, payloadLen = 0;
    UINT32 declaredChksum = 0, actualChksum = 0;

    // check for valid length
    if ( p->pos < OSP_OVERHEAD_SIZE ) {
        WARN("message too short");
        PRINT_DATA(buff, len);
        return EXIT_FAILURE;
    }
    
    // payload length
    payloadLen |= ( buff[i++] << 8 );
    payloadLen |= ( buff[i++] );
    
    // compare length
    if ( payloadLen != (len-4) ) {
        WARN("checksum failed (%d != %d)", payloadLen, (len-4));
        PRINT_DATA(buff, len);
        return EXIT_FAILURE;
    }

    // calc checksum
    while ( i < len-2 ) {
        actualChksum += buff[i++];
    }
    actualChksum &= 0x7FFF;
    
    declaredChksum |= ( buff[i++] << 8 );
    declaredChksum |= ( buff[i++] );
    
    // compare checksum
    if ( declaredChksum != actualChksum ) {
        WARN("checksum failed (0x%x != 0x%x)", declaredChksum, actualChksum);
        PRINT_DATA(buff, len);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

// message start: 0xa0 0xa2
// message end: 0xb0 0xb3
void osp_parser_read( OspParser* p, UINT8* buff, UINT32 len )
{
    UINT32 nn;
    for (nn = 0; nn < len; nn++) {
    
        // ignore input once we've overflowed, until we get 0xb3
        if (p->overflow) {
            if ( buff[nn] == OSP_END_BYTE_2 )
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

        // ignore new input until we get 0xa0 0xa2
        if ( ( p->pos == 1 ) && ( buff[nn] != OSP_START_BYTE_2 ) ) {
            p->pos = 0;
            if ( buff[nn] != OSP_START_BYTE_1 ) {
                WARN("dropping 0x%x", buff[nn]);
                continue;
            }
        } else if ( ( p->pos == 0 ) && ( buff[nn] != OSP_START_BYTE_1 ) ) {
            WARN("dropping 0x%x", buff[nn]);
            continue;
        }
        
        p->in[p->pos] = buff[nn];
        p->pos++;

        if (( p->pos >= 2 ) &&
            ( p->in[p->pos -1] == OSP_END_BYTE_2) &&
            ( p->in[p->pos -2] == OSP_END_BYTE_1) ) 
        {
#if DBG_OSP
    DEBUG("%d bytes", p->pos);
    PRINT_DATA( p->in, p->pos );
#endif
            if (OK( _osp_parser_validate( p, p->in + 2, p->pos - 4 ) ))
                _osp_parser_parse( p, p->in + 4, p->pos - 8 );
                
            p->pos = 0;
        }
    }
}

OspParser* osp_parser_init( 
    MessageQueue* msgQueue
) {
    OspParser* p = (OspParser*) calloc( 1, sizeof( OspParser ) );
    p->msgQueue = msgQueue;
    return p;
}

void osp_parser_free( OspParser* p )
{
    if ( !p ) return;
    free( p );
}
