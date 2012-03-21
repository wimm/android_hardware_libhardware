/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _NMEA_MSG_H_
#define _NMEA_MSG_H_

#include "buffer.h"
#include "message.h"
#include "nmea_types.h"

EXIT_CODE nmea_msg_read( Message* msg, Buffer* buff );
EXIT_CODE nmea_msg_write( Message* msg, Buffer* buff );

#endif /* _NMEA_MSG_H_ */
