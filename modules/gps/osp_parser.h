/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _OSP_PARSER_H_
#define _OSP_PARSER_H_

#include "gps.h"
#include "message_queue.h"
#include "osp_types.h"

typedef struct {
    UINT32  pos;
    UINT32  overflow;
    UINT8   in[ OSP_MAX_SIZE+1 ];
    
    MessageQueue*       msgQueue;
} OspParser;

OspParser* osp_parser_init( 
    MessageQueue* msgQueue
);

void osp_parser_free( OspParser* p );

void osp_parser_read( OspParser* p, UINT8* buff, UINT32 len );

#endif /* _OSP_PARSER_H_ */
