/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */

 
#ifndef _BUFFER_H_
#define _BUFFER_H_

#include "gps.h"

typedef struct {
    UINT8* data;
    UINT32 len;
    UINT32 pos;
} Buffer;
    

/* read_data
 *
 * Read "length" bytes into "data" starting at the buffer's
 * current position.
 *
 * Returns EXIT_FAILURE if reading would go outside the buffer.
 */
EXIT_CODE read_data( Buffer* b, void* data, UINT16 length );

/* read_int
 *
 * Read an integer of "length" bytes starting at the buffer's
 * current position.  Place the result into "result" and increment "pos".
 *
 * Returns EXIT_FAILURE if reading would go outside the buffer or 
 * if "length" is greater than the size of "result".
 */
EXIT_CODE read_int( Buffer* b, UINT64* result, UINT8 length );

/* write_data
 *
 * Write "length" bytes from "data" starting at the buffer's
 * current position.
 *
 * Returns EXIT_FAILURE if writing would go outside the buffer.
 */
EXIT_CODE write_data( Buffer* b, void* data, UINT16 length );


EXIT_CODE write_format( Buffer* b, const char *fmt, ... );

/* write_int
 *
 * Write the integer "value"  of "length" bytes starting at the buffer's
 * current position.
 *
 * Returns EXIT_FAILURE if writing would go outside the buffer.
 */
EXIT_CODE write_int( Buffer* b, UINT64 value, UINT8 length );



/* READ_INT
 * 
 * Convenience macro to handle integer casting and failure logging.
 * Pass in a pointer to a Buffer, a variable and a byte length.
 */
#define READ_INT(__b,__var,__len)   \
({ \
    EXIT_CODE __res = EXIT_FAILURE; \
    UINT64 __val; \
    if (FAILED( read_int( (__b), &__val, (__len) ) )) { \
        ERROR("failed to read " #__var); \
    } else { \
        __res = EXIT_SUCCESS; \
        __var = __val; \
    } \
    __res; \
})


/* READ_VAR
 * 
 * Convenience macro to handle integer casting and failure logging.
 * Pass in a pointer to a Buffer and a variable.
 */
#define READ_VAR(__b,__var)   \
    READ_INT(__b,__var,sizeof(__var))


/* WRITE_INT
 * 
 * Convenience macro to handle integer casting and failure logging.
 * Pass in a pointer to a Buffer, a variable and a byte length.
 */
#define WRITE_INT(__b,__var,__len)   \
({ \
    EXIT_CODE __res = EXIT_FAILURE; \
    if (FAILED( write_int( (__b), (__var), (__len) ) )) { \
        ERROR("failed to write " #__var); \
    } else { \
        __res = EXIT_SUCCESS; \
    } \
    __res; \
})


/* WRITE_VAR
 * 
 * Convenience macro to handle integer casting and failure logging.
 * Pass in a pointer to a Buffer and a variable.
 */
#define WRITE_VAR(__b,__var)   \
    WRITE_INT(__b,__var,sizeof(__var))

#endif /* _BUFFER_H_ */
