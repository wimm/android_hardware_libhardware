/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _OSP_MSG_H_
#define _OSP_MSG_H_

#include "buffer.h"
#include "gps_comm.h"
#include "message.h"
#include "osp_types.h"

EXIT_CODE osp_msg_read( Message* msg, Buffer* buff );
EXIT_CODE osp_msg_write( Message* msg, Buffer* buff );

#endif /* _OSP_MSG_H_ */
