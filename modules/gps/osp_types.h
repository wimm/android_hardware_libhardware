/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _OSP_TYPES_H_
#define _OSP_TYPES_H_

#include "gps.h"

/*
 * CONSTANTS
 */
 
#define OSP_OVERHEAD_SIZE       8           // 6 = start(2) length(2) chksum(2) end(2)
#define OSP_DATA_SIZE           ((1<<11)-1) // 2047
#define OSP_MAX_SIZE            (OSP_DATA_SIZE+OSP_OVERHEAD_SIZE)

#define OSP_START_BYTE_1        0xa0
#define OSP_START_BYTE_2        0xa2

#define OSP_END_BYTE_1          0xb0
#define OSP_END_BYTE_2          0xb3


#define OSP_TIMEOUT_MS              3000

#define ECLM_ACK_TIMEOUT_MS     15000
 
#define OSP_SW_VERSION_STRING_LEN  80

void osp_msg_dispose( void* msg );

    
/*
 * MESSAGE IDS
 */

typedef UINT16  OspId;
typedef UINT8   OspMajorId;
typedef UINT8   OspMinorId;

#define OSP_ID( _major, _minor )            (( (_major) << 8 ) | ( (_minor) & 0xFF ))
#define OSP_ID_MAJOR( _id )                 ( (_id) >> 8 )
#define OSP_ID_MINOR( _id )                 ( (_id) & 0xFF )

#define OSP_MID_MEASURED_NAVIGATION_DATA    0x02
    #define OSP_ID_MEASURED_NAVIGATION_DATA OSP_ID(OSP_MID_MEASURED_NAVIGATION_DATA, 0)

#define OSP_MID_MEASURED_TRACKER_DATA       0x04
    #define OSP_ID_MEASURED_TRACKER_DATA    OSP_ID(OSP_MID_MEASURED_TRACKER_DATA, 0)

#define OSP_MID_SW_VERSION                  0x06
    #define OSP_ID_SW_VERSION               OSP_ID(OSP_MID_SW_VERSION, 0)

#define OSP_MID_CLOCK_STATUS_DATA           0x07
    #define OSP_ID_CLOCK_STATUS_DATA        OSP_ID(OSP_MID_CLOCK_STATUS_DATA, 0)

#define OSP_MID_CPU_THROUGHPUT              0x09
    // UNUSED

#define OSP_MID_COMMAND_ACK                 0x0B
    #define OSP_ID_COMMAND_ACK              OSP_ID(OSP_MID_COMMAND_ACK, 0)

#define OSP_MID_COMMAND_NACK                0x0C
    #define OSP_ID_COMMAND_NACK             OSP_ID(OSP_MID_COMMAND_NACK, 0)

#define OSP_MID_OK_TO_SEND                  0x12
    #define OSP_ID_OK_TO_SEND               OSP_ID(OSP_MID_OK_TO_SEND, 0)

#define OSP_MID_GEODETIC_NAVAGATION         0x29
    #define OSP_ID_GEODETIC_NAVAGATION      OSP_ID(OSP_MID_GEODETIC_NAVAGATION, 0)

#define OSP_MID_EE_OUTPUT                   0x38
    #define OSP_ID_DATA_AND_EPHEMERIS_REQ   OSP_ID(OSP_MID_EE_OUTPUT, 0x01)
    #define OSP_ID_EE_INTEGRITY             OSP_ID(OSP_MID_EE_OUTPUT, 0x02)
    #define OSP_ID_EPHEMERIS_STATUS_RESP    OSP_ID(OSP_MID_EE_OUTPUT, 0x03)
    #define OSP_ID_VERIFIED_BE_DATA         OSP_ID(OSP_MID_EE_OUTPUT, 0x05)
    #define OSP_ID_ECLM_ACK_NACK            OSP_ID(OSP_MID_EE_OUTPUT, 0x20)
    #define OSP_ID_EE_AGE_RESP              OSP_ID(OSP_MID_EE_OUTPUT, 0x21)
    #define OSP_ID_SGEE_AGE_RESP            OSP_ID(OSP_MID_EE_OUTPUT, 0x22)
    #define OSP_ID_ECLM_DOWNLOAD_REQ        OSP_ID(OSP_MID_EE_OUTPUT, 0x23)
    #define OSP_ID_HOST_FILE_ERASE          OSP_ID(OSP_MID_EE_OUTPUT, 0x24)
    #define OSP_ID_HOST_FILE_UPDATE         OSP_ID(OSP_MID_EE_OUTPUT, 0x25)
    #define OSP_ID_HOST_FILE_REQ            OSP_ID(OSP_MID_EE_OUTPUT, 0x26)
    #define OSP_ID_STORE_EE_HEADER          OSP_ID(OSP_MID_EE_OUTPUT, 0x27)
    #define OSP_ID_FETCH_EE_HEADER          OSP_ID(OSP_MID_EE_OUTPUT, 0x28)
    #define OSP_ID_SIF_AIDING_STATUS_RESP   OSP_ID(OSP_MID_EE_OUTPUT, 0x29)
    #define OSP_ID_EE_ACK_NACK              OSP_ID(OSP_MID_EE_OUTPUT, 0xFF)

#define OSP_MID_CW_OUTPUT                   0x5C
    // UNUSED

#define OSP_MID_TCXO_LEARNING_OUT           0x5D
    // UNUSED
    
#define OSP_MID_SET_NMEA_MODE               0x81
    #define OSP_ID_SET_NMEA_MODE            OSP_ID(OSP_MID_SET_NMEA_MODE, 0)

#define OSP_MID_POLL_SW_VERSION             0x84
    #define OSP_ID_POLL_SW_VERSION          OSP_ID(OSP_MID_POLL_SW_VERSION, 0)
    
#define OSP_MID_SET_SERIAL_PORT             0x86
    #define OSP_ID_SET_SERIAL_PORT          OSP_ID(OSP_MID_SET_SERIAL_PORT, 0)

#define OSP_MID_SW_TOOLBOX                  0xB2
    #define OSP_ID_PM_STORAGE_CONTROL       OSP_ID(OSP_MID_SW_TOOLBOX, 0x14)
    #define OSP_ID_PM_LOAD_REQ              OSP_ID(OSP_MID_SW_TOOLBOX, 0x22)
    #define OSP_ID_PM_EXIT_REQ              OSP_ID(OSP_MID_SW_TOOLBOX, 0x26)
    #define OSP_ID_PM_START_REQ             OSP_ID(OSP_MID_SW_TOOLBOX, 0x28)
    #define OSP_ID_PM_PROMPT                OSP_ID(OSP_MID_SW_TOOLBOX, 0x90)
    #define OSP_ID_PM_ACK                   OSP_ID(OSP_MID_SW_TOOLBOX, 0x91)

#define OSP_MID_EE_INPUT                    0xE8
    #define OSP_ID_EPHEMERIS_STATUS_REQ     OSP_ID(OSP_MID_EE_INPUT, 0x02)
    #define OSP_ID_ECLM_START_DOWNLOAD      OSP_ID(OSP_MID_EE_INPUT, 0x16)
    #define OSP_ID_SGEE_DL_FILE_SIZE        OSP_ID(OSP_MID_EE_INPUT, 0x17)
    #define OSP_ID_SGEE_DL_PKT_DATA         OSP_ID(OSP_MID_EE_INPUT, 0x18)
    #define OSP_ID_EE_AGE_REQ               OSP_ID(OSP_MID_EE_INPUT, 0x19)
    #define OSP_ID_SGEE_AGE_REQ             OSP_ID(OSP_MID_EE_INPUT, 0x1A)
    #define OSP_ID_HOST_FILE_CONTENT        OSP_ID(OSP_MID_EE_INPUT, 0x1B)
    #define OSP_ID_HOST_ACK_NACK            OSP_ID(OSP_MID_EE_INPUT, 0x1C)
    #define OSP_ID_EE_HEADER_RESP           OSP_ID(OSP_MID_EE_INPUT, 0x1E)
    #define OSP_ID_DISABLE_SIF_AIDING       OSP_ID(OSP_MID_EE_INPUT, 0x20)
    #define OSP_ID_SIF_AIDING_STATUS_REQ    OSP_ID(OSP_MID_EE_INPUT, 0x21)
    #define OSP_ID_PARTIAL_GEOTAG_CONV_REQ  OSP_ID(OSP_MID_EE_INPUT, 0xD0)
    #define OSP_ID_EE_STORAGE_CONTROL       OSP_ID(OSP_MID_EE_INPUT, 0xFD)
    #define OSP_ID_DISABLE_CGEE_PREDICTION  OSP_ID(OSP_MID_EE_INPUT, 0xFE)
    #define OSP_ID_EE_DEBUG                 OSP_ID(OSP_MID_EE_INPUT, 0xFF)


typedef UINT8 OspNmeaDebugMode;
enum _OspNmeaDebugMode {
    OSP_NMEA_DEBUG_ENABLE                   = 0x0,
    OSP_NMEA_DEBUG_DISABLE                  = 0x1,
    OSP_NMEA_DEBUG_NO_CHANGE                = 0x2
};

/*
 * TYPES
 */

// for OSP_ID_OK_TO_SEND
typedef UINT8 OspOkToSendVal;
enum _OspOkToSendVal {
    SIRF_NOT_OK_TO_SEND         = 0,
    SIRF_OK_TO_SEND             = 1
};
 
// for OSP_ID_MEASURED_TRACKER_DATA
typedef struct {
    UINT8 prn;
    UINT8 azimuth;
    UINT8 elevation;
    UINT16 state;
    UINT8 snr[10];
} OspTrackerChannelInfo;

    
/*
 * MESSAGES
 */
 
// OSP_ID_MEASURED_NAVIGATION_DATA
typedef struct {
    UINT32 xPos;
    UINT32 yPos;
    UINT32 zPos;
    UINT16 xVel;
    UINT16 yVel;
    UINT16 zVel;
    UINT8 mode1;
    UINT8 hdop2;
    UINT8 mode2;
    UINT16 gpsWeek;
    UINT32 gpsTimeOfWeek;
    UINT8 svsUsedInFix;
    UINT8 chPrn[GPS_CHANNEL_MAX];
} OspMeasuredNavigationData;

// OSP_ID_MEASURED_TRACKER_DATA
typedef struct {
    UINT16 gpsWeek;
    UINT32 gpsTimeOfWeek;
    UINT8 channels;
    OspTrackerChannelInfo channel[GPS_CHANNEL_MAX];
} OspMeasuredTrackerData;

// OSP_ID_SW_VERSION
typedef struct {
    char* sirfVersion;
    char* customerVersion;
} OspSwVersion;

// OSP_ID_COMMAND_ACK
typedef struct {
    OspId msgId;
} OspCommandAck;

// OSP_ID_COMMAND_NACK
typedef struct {
    OspId msgId;
} OspCommandNack;

// OSP_ID_OK_TO_SEND
typedef struct {
    OspOkToSendVal okToSend;
} OspOkToSend;

// OSP_ID_SET_NMEA_MODE
typedef struct {
    OspNmeaDebugMode debugMode;
    UINT8 ggaRate;
    UINT8 gllRate;
    UINT8 gsaRate;
    UINT8 gsvRate;
    UINT8 rmcRate;
    UINT8 vtgRate;
    UINT8 mssRate;
    UINT8 epeRate;
    UINT8 zdaRate;
    UINT16 baudRate;
} OspSetNmeaMode;

// OSP_ID_POLL_SW_VERSION
// data unused

// OSP_ID_SET_SERIAL_PORT
typedef struct {
    UINT32 bitRate;
} OspSerialConfig;


// OSP_ID_STORE_EE_HEADER
typedef struct {
    UINT16 dataOffset;
    UINT16 dataLen;
    UINT8* data;
} OspStoreEeHeader;

// OSP_ID_EE_HEADER_RESP
typedef struct {
    UINT16 seqNum;
    UINT16 dataLen;
    UINT32 dataOffset;
    UINT8* data;
} OspEeHeaderResp;


#endif /* _OSP_TYPES_H_ */
