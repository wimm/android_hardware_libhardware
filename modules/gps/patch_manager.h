/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _PATCH_MANAGER_H_
#define _PATCH_MANAGER_H_

#include "work_thread.h"

#define GPS_PATCH_PATH              GPS_ROOT_PATH "/patch.pd2"
#define GPS_PATCH_RATE              GPS_BAUD_57600

EXIT_CODE pm_send_patch_file( WorkState* s, const char* path );

#endif /* _PATCH_MANAGER_H_ */
