/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _NMEA_TYPES_H_
#define _NMEA_TYPES_H_

#include "gps_datetime.h"
#include "gps_state.h"


#define NMEA_MAX_SIZE           1024
#define NMEA_OVERHEAD_SIZE      6   // 6 = $ + * + chksum + \r\n
#define NMEA_DATA_SIZE          (NMEA_MAX_SIZE-NMEA_OVERHEAD_SIZE)



/*
 * GEO TYPES
 */

#define NMEA_GEO_MSG_PREFIX             "GP"

enum _NmeaGeoMessageId {
    NMEA_GEO_GGA,
    NMEA_GEO_GLL,
    NMEA_GEO_GSA,
    NMEA_GEO_GSV,
    NMEA_GEO_MSS,
    NMEA_GEO_RMC,
    NMEA_GEO_VTG,
    NMEA_GEO_ZDA,
};

typedef int NmeaGeoCalculationMode;
enum _NmeaGeoCalculationMode {
    NMEA_GEO_CALC_MODE_INVALID            = 0,
    NMEA_GEO_CALC_MODE_GPS_SPS            = 1,
    NMEA_GEO_CALC_MODE_DIFFERENTIAL_SPS   = 2,
    NMEA_GEO_CALC_MODE_DEAD_RECKONING     = 6
};

typedef char NmeaGeoFixMode;
enum _NmeaGeoFixMode {
    NMEA_GEO_FIX_MODE_AUTONOMOUS         = 'A',
    NMEA_GEO_FIX_MODE_DGPS               = 'D',
    NMEA_GEO_FIX_MODE_DR                 = 'E',
    NMEA_GEO_FIX_MODE_INVALID            = 'N',
    NMEA_GEO_FIX_MODE_COARSE             = 'R',
    NMEA_GEO_FIX_MODE_SIMULATOR          = 'S'
};

typedef char NmeaGeoValidityFlag;
enum _NmeaGeoValidityFlag {
    NMEA_GEO_FLAG_INVALID               = 'V',
    NMEA_GEO_FLAG_VALID                 = 'A'
};

typedef char NmeaGeoFixPrecisionMode;
enum _NmeaGeoFixPrecisionMode {
    NMEA_GEO_PRECISION_MODE_MANUAL       = 'M',
    NMEA_GEO_PRECISION_MODE_AUTOMATIC    = 'A'
};

typedef int NmeaGeoFixPrecisionFlag;
enum _NmeaGeoFixPrecisionFlag {
    NMEA_GEO_PRECISION_FLAG_NO_FIX        = 1,
    NMEA_GEO_PRECISION_FLAG_2D            = 2,
    NMEA_GEO_PRECISION_FLAG_3D            = 3
};

// NMEA_GEO_GGA
typedef struct {
    GpsTime time;
    double lat;
    double lon;
    NmeaGeoCalculationMode calcMode;
    int satCount;
    float hdop;
    double altitude;
    float geoidSep;
} NmeaGeoGGA;

// NMEA_GEO_GLL
typedef struct {
    double lat;
    double lon;
    GpsTime time;
    NmeaGeoValidityFlag valid;
    NmeaGeoFixMode fixMode;
} NmeaGeoGLL;

// NMEA_GEO_GSA
typedef struct {
    NmeaGeoFixPrecisionMode mode;
    NmeaGeoFixPrecisionFlag flag;
    UINT8 satUsed[GPS_MAX_SVS];
    float pdop;
    float hdop;
    float vdop;
} NmeaGeoGSA;

// NMEA_GEO_GSV
typedef struct {
    int prn;
    int elevation;
    int azimuth;
    int snr;
} NmeaGeoSatInfo;

typedef struct {
    int msgCount;
    int msgIndex;
    int satInView;
    NmeaGeoSatInfo satInfo[GPS_MAX_SVS];
} NmeaGeoGSV;

// NMEA_GEO_MSS
typedef struct {
    int strength;
    int snr;
    float freq;
    int bps;
    int channel;
} NmeaGeoMSS;

// NMEA_GEO_RMC
typedef struct {
    GpsTime time;
    NmeaGeoValidityFlag valid;
    double lat;
    double lon;
    float speed;
    float bearing;
    GpsDate date;
    NmeaGeoFixMode fixMode;
} NmeaGeoRMC;

// NMEA_GEO_VTG
typedef struct {
    float bearingTrue;
    float bearingMagnetic;
    float speedKnots;
    float speedKm;
    NmeaGeoFixMode fixMode;
} NmeaGeoVTG;

// NMEA_GEO_ZDA
typedef struct {
    GpsTime time;
    GpsDate date;
} NmeaGeoZDA;



/*
 * SIRF TYPES
 */

#define NMEA_SIRF_MSG_PREFIX         "PSRF"

typedef UINT8 SirfMessageId;
enum _SirfMessageId {
    // Input
    SIRF_I_SET_SERIAL_PORT   = 100,
    SIRF_I_NAV_INIT          = 101,
    SIRF_I_SET_DGPS_PORT     = 102,
    SIRF_I_QUERY_OR_SET_RATE = 103,
    SIRF_I_LLA_NAV_INIT      = 104,
    SIRF_I_SET_DEBUG         = 105,
    SIRF_I_SET_DATUM         = 106,
    SIRF_I_EE_PROPRIETARY_1  = 107,
    SIRF_I_EE_PROPRIETARY_2  = 108,
    SIRF_I_SET_EE_DEBUG      = 110,
    SIRF_I_SET_MESSAGE_RATE  = 112,
    SIRF_I_EE_ECLM_MSG       = 114,
    SIRF_I_SYSTEM_MSG        = 117,
    SIRF_I_STORAGE_CONFIG    = 120,
    SIRF_I_SOFTWARE_CONFIG   = 200,
    
    // Output
    SIRF_O_OK_TO_SEND        = 150,
    SIRF_O_DATA_AND_EE_REQ   = 151,
    SIRF_O_EE_INTEGRITY      = 152,
    SIRF_O_EE_ACK            = 154,
    SIRF_O_EE_PROPRIETARY_1  = 155,
    SIRF_O_EE_ECLM_MSG       = 156,
    SIRF_O_EXCEPTION         = 160,
    SIRF_O_RESERVED_1        = 255,
};

typedef UINT8 SirfResetMode;
enum _SirfResetMode {
    /* Use initialization data and begin in start 
     * mode. Uncertainties are 5 seconds time 
     * accuracy and 300 km position accuracy. 
     * Ephemeris data in SRAM is used. */
    SIRF_RESET_HOT              = 1,
    
    /* Ephemeris data is cleared, and warm start 
     * performed using remaining data in RAM. 
     * No initialization data is used. */
    SIRF_RESET_WARM_NO_INIT     = 2,
    
    /* Initialization data is used, ephemeris data 
     * is cleared, and warm start performed using 
     * remaining data in RAM. */
    SIRF_RESET_WARM_WITH_INIT   = 3,
    
    /* Position, time, and ephemeris are cleared, 
     * and a cold start is performed. 
     * No initialization data is used. */
    SIRF_RESET_COLD             = 4,
    
    /* Internal RAM is cleared and a factory reset 
     * is performed. 
     * No initialization data is used. */
    SIRF_RESET_FACTORY          = 8
};

typedef UINT8 SirfQueryMsg;
enum _SirfQueryMsg {
    SIRF_QUERY_GGA              = 0,
    SIRF_QUERY_GLL              = 1,
    SIRF_QUERY_GSA              = 2,
    SIRF_QUERY_GSV              = 3,
    SIRF_QUERY_RMC              = 4,
    SIRF_QUERY_VTG              = 5,
    SIRF_QUERY_MSS              = 6,
    SIRF_QUERY_ZDA              = 8,
};

typedef UINT8 SirfQueryMode;
enum _SirfQueryMode {
    SIRF_QMODE_SET_RATE         = 0,
    SIRF_QMODE_ONE_TIME         = 1,
    SIRF_QMODE_ABP_ON           = 2,
    SIRF_QMODE_ABP_OFF          = 3
};

typedef UINT8 SirfCksumEnable;
enum _SirfCksumEnable {
    SIRF_CKSUM_DISABLE          = 0,
    SIRF_CKSUM_ENABLE           = 1
};

typedef UINT8 SirfDebugEnable;
enum _SirfDebugEnable {
    SIRF_DEBUG_DISABLE          = 0,
    SIRF_DEBUG_ENABLE           = 1
};

typedef UINT8 SirfPatchStorage;
enum _SirfPatchStorage {
    SIRF_PATCHSTORE_NO_CHANGE   = '0',
    SIRF_PATCHSTORE_I2C         = 'F',
    SIRF_PATCHSTORE_NO_I2C      = 'N'
};

typedef UINT8 SirfEEStorage;
enum _SirfEEStorage {
    SIRF_EESTORE_NO_CHANGE      = '0',
    SIRF_EESTORE_NO_STORAGE     = 'N',
    SIRF_EESTORE_FLASH          = 'F',
    SIRF_EESTORE_EEROM          = 'R',
    SIRF_EESTORE_HOST           = 'H'
};

// SIRF_O_DATA_AND_EE_REQ / OSP_ID_DATA_AND_EPHEMERIS_REQ
typedef struct {
    UINT8 gpsTimeValidity;
    UINT16 gpsWeek;
    float gpsTimeOfWeek;
    UINT32 ephReqMask;
} SirfEeRequest;

// SIRF_O_EE_INTEGRITY / OSP_ID_EE_INTEGRITY
typedef struct {
    UINT32 satPosValidity;
    UINT32 satClkValidity;
    UINT32 satHealth;
} SirfEeIntegrity;

// SIRF_O_EE_ACK / OSP_ID_EE_ACK_NACK
typedef struct {
    UINT8 ackMsgId;
} SirfEeAck;

// SIRF_I_SET_SERIAL_PORT
typedef struct {
    GpsProtocol protocol;
    UINT32 bitRate;
} SirfSerialConfig;

// SIRF_I_LLA_NAV_INIT
typedef struct {
    GpsLocation location;
    UINT32 clockDrift;
    GpsWeekDateTime dateTime;
    UINT8 channelCount;
    SirfResetMode resetMode;
} SirfLlaNavInit;


typedef UINT8 EclmMessageId;
enum _EclmMessageId {
    ECLM_I_START_DL        = 0x16, // ack
    ECLM_I_FILE_SIZE       = 0x17, // ack
    ECLM_I_PACKET_DATA     = 0x18, // ack
    ECLM_I_EE_AGE_REQ      = 0x19, // ack
    ECLM_I_SGEE_AGE_REQ    = 0x1a, // ack
    ECLM_I_HOST_FILE       = 0x1b,
    ECLM_I_ACK_NACK        = 0x1c,
    
    ECLM_O_ACK_NACK        = 0x20,
    ECLM_O_EE_AGE_RESP     = 0x21,
    ECLM_O_SGEE_AGE_RESP   = 0x22,
    ECLM_O_SGEE_REQ        = 0x23, // ack
    ECLM_O_FILE_ERASE      = 0x24, // ack
    ECLM_O_FILE_UPDATE     = 0x25, // ack
    ECLM_O_FILE_REQ        = 0x26,
};

#endif /* _NMEA_TYPES_H_ */
