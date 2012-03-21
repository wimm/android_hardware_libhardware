/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */

#include "buffer.h"

EXIT_CODE read_data( Buffer* b, void* data, UINT16 length )
{
    if ( b->pos + length > b->len ) {
        WARN("underrun: pos=%d len=%d requested=%d", b->pos, b->len, length);
        return EXIT_FAILURE;
    }
    
    memcpy( data, b->data + b->pos, length );
    b->pos += length;
    
    return EXIT_SUCCESS;
}

EXIT_CODE read_int( Buffer* b, UINT64* result, UINT8 length ) 
{
    if ( result == NULL ) {
        WARN("result can not be NULL");
        return EXIT_FAILURE;
    }

    if ( length > sizeof( *result ) ) {
        WARN("overflow: pos=%d len=%d requested=%d", b->pos, b->len, length);
        return EXIT_FAILURE;
    }

    if ( b->pos + length > b->len ) {
        WARN("underrun: pos=%d len=%d requested=%d", b->pos, b->len, length);
        return EXIT_FAILURE;
    }
    
    *result = 0;
    while ( length-- > 0 ) {
        *result = ( *result << 8 ) | b->data[b->pos++];
    }
    
    return EXIT_SUCCESS;
}

EXIT_CODE write_int( Buffer* b, UINT64 value, UINT8 length ) 
{
    if ( b->pos + length > b->len ) {
        WARN("overflow: pos=%d len=%d requested=%d", b->pos, b->len, length);
        return EXIT_FAILURE;
    }
    
    while ( length-- > 0 ) {
        b->data[b->pos++] = ( value >> (length*8) ) & 0xFF;
    }
    
    return EXIT_SUCCESS;
}

EXIT_CODE write_data( Buffer* b, void* data, UINT16 length ) 
{
    if ( b->pos + length > b->len ) {
        WARN("overflow: pos=%d len=%d requested=%d", b->pos, b->len, length);
        return EXIT_FAILURE;
    }
    
    memcpy( b->data + b->pos, data, length );
    b->pos += length;
    
    return EXIT_SUCCESS;
}

EXIT_CODE write_format( Buffer* b, const char *fmt, ... ) 
{
	va_list var_args;
	int len, rem;
	char *str;
	
	str = (char*) b->data + b->pos;
	rem = b->len - b->pos;

	va_start(var_args, fmt);
	len = vsnprintf(str, rem, fmt, var_args);
	va_end(var_args);

	if ( len >= rem ) {
	    ERROR("overflow %d bytes pos=%d len=%d", (len-rem)+1, b->pos, b->len);
	    return EXIT_FAILURE;
    }
	
    b->pos += len;
    return EXIT_SUCCESS;
}
