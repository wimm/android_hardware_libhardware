/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _GPS_H_
#define _GPS_H_


#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <hardware/gps.h>


/*
 * CONSTANTS
 */

#define GPS_DEBUG                   1
#define GPS_DEV_PATH                "/dev/s3c2410_serial2"
#define GPS_PULSE_ON_OFF_PATH       "/sys/devices/platform/sirf-gsd4e/pulse_on_off"

#define GPS_ROOT_PATH               "/data/gps"
#define GPS_PATCH_PATH              GPS_ROOT_PATH "/patch.pd2"
#define GPS_STORAGE_PATH            GPS_ROOT_PATH "/nvm"
#define GPS_SGEE_PATH               GPS_ROOT_PATH "/sgee.bin"

#define GPS_NVM_STORAGE_SGEE        GPS_STORAGE_PATH "/server"
#define GPS_NVM_STORAGE_CGEE        GPS_STORAGE_PATH "/client"
#define GPS_NVM_STORAGE_BE          GPS_STORAGE_PATH "/broadcast"

#define GPS_WAIT_FOR_POWER_DELAY    10 // ms
#define GPS_WAIT_FOR_IDLE_DELAY     900 // ms
#define GPS_WAIT_FOR_MSG_DELAY      1200 // ms

#define GPS_CLOCK_DRIFT             96250
#define GPS_CHANNEL_MAX             12


/*
 * TYPES
 */

#include <sys/types.h>
#ifndef UINT8
#define UINT8       uint8_t
#endif
#ifndef UINT16
#define UINT16      uint16_t
#endif
#ifndef UINT32
#define UINT32      uint32_t
#endif
#ifndef UINT64
#define UINT64      uint64_t
#endif
#ifndef FILE_DESC
#define FILE_DESC   int
#endif


/*
 * CONVENIENCE
 */

typedef int EXIT_CODE;
#define OK(_x)              ((_x)==EXIT_SUCCESS)
#define FAILED(_x)          ((_x)==EXIT_FAILURE)
#define FAIL_UNLESS(_x)     ({if((_x)==EXIT_FAILURE) return EXIT_FAILURE;})

// MIN
#define MIN(a,b)  (((a) < (b)) ? (a) : (b))

// TIME
typedef int64_t TIME;

// NOW
// returns current time in milliseconds
#define NOW() ({                    \
    int64_t __t;                    \
    struct timeval __tv;            \
    gettimeofday(&__tv,NULL);       \
    __t = (__tv.tv_sec * 1000LL +   \
           __tv.tv_usec / 1000);    \
    __t; })
    
    
// COPY
// allocates and returns a copy of the struct
#define COPY(__st)                                  \
    ({                                              \
        void *__res = malloc( sizeof((__st)) );     \
        memcpy( __res, &(__st), sizeof((__st)) );   \
        __res;                                      \
    })


/*
 * LOGGING
 */

// NMEA protocol I/O
#define DBG_NMEA        0

// OSP protocol I/O
#define DBG_OSP         0

// message parse and send
#define DBG_MSG_IO      0

// message lifecycle events
#define DBG_MSG_QUEUE   0

// gps geographic and satellite 
#define DBG_GEO         1

// patch file info 
#define DBG_PATCH       0

// EE info 
#define DBG_EE          1


#define LOG_TAG  "gps_wimm"
#include <cutils/log.h>

#define ERROR(fmt, arg...)          LOGE("[%d][%s] " fmt, gettid(), __func__, ## arg)
#define WARN(fmt, arg...)           LOGW("[%d][%s] " fmt, gettid(), __func__, ## arg)
#define INFO(fmt, arg...)           LOGI("[%d][%s] " fmt, gettid(), __func__, ## arg)

#if GPS_DEBUG
#define DEBUG(fmt, arg...)          LOGD("[%d][%s] " fmt, gettid(), __func__, ## arg)
#else
#define DEBUG(...)                  ((void)0)
#endif

static void PRINT_DATA(UINT8* buf, UINT32 len) {
    char *s;
    char temp[48];
    UINT32 i = 0, j;
    while (i<len) {
        s = temp;
        s += sprintf( s, "%04X: ", i );
        for (j=0; j<8 && i<len; j++) {
            s += sprintf( s, "0x%02X ", buf[i++] );
        }
        android_writeLog(ANDROID_LOG_INFO, LOG_TAG, temp);
    }
}

#endif /* _GPS_H_ */
