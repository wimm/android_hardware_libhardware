/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _WORK_THREAD_SIRF_H_
#define _WORK_THREAD_SIRF_H_

#include "work_thread.h"
#include "nmea_types.h"

EXIT_CODE sirf_set_comm( WorkState* s, GpsProtocol protocol, GpsBaudRate rate );

EXIT_CODE sirf_init_nav( WorkState* s, SirfResetMode resetMode );

EXIT_CODE sirf_log_sw_version( WorkState* s );

EXIT_CODE sirf_poll_sgee_age( WorkState* s );

EXIT_CODE sirf_patch_rom( WorkState* s );

EXIT_CODE sirf_wait_for_ok_to_send( WorkState* s );

#endif /* _WORK_THREAD_SIRF_H_ */
