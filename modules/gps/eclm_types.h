/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _ECLM_TYPES_H_
#define _ECLM_TYPES_H_

#include "gps.h"

/*
 * ECLM TYPES
 * (Embedded Client Location Manager)
 */


/*
 * CONSTANTS
 */
typedef UINT8 EclmAckNack;
enum _EclmAckNack {
    ECLM_ACK                    = 0x0,
    ECLM_NACK                   = 0x1
};

typedef UINT8 EclmStorageId;
enum _EclmStorageId {
    ECLM_STORAGE_SGEE               = 1,
    ECLM_STORAGE_CGEE               = 2,
    ECLM_STORAGE_BE                 = 3
};

typedef UINT8 EclmEphPos;
enum _EclmEphPos {
    ECLM_EPH_INVALID       = 0,
    ECLM_EPH_BE            = 1,
    ECLM_EPH_SGEE          = 2,
    ECLM_EPH_CGEE          = 3
};

typedef UINT8 EclmResultIn;
enum _EclmResultIn {
    ECLM_RES_I_SUCCESS           = 0x00,
    ECLM_RES_I_INVALID_NVM       = 0x01,
    ECLM_RES_I_FILE_ACCESS_ERR   = 0x13
};

typedef UINT8 EclmResultOut;
enum _EclmResultOut {
    ECLM_RES_O_SUCCESS             = 0x00,
    ECLM_RES_O_SPACE_UNAVAIL       = 0x01,
    ECLM_RES_O_PKT_LEN_INAVLID     = 0x02,
    ECLM_RES_O_PKT_OUT_OF_SEQ      = 0x03,
    ECLM_RES_O_DL_SGEE_NO_NEW_FILE = 0x04,
    ECLM_RES_O_DL_CORRUPT_FILE     = 0x05,
    ECLM_RES_O_DL_GENERIC_FAILURE  = 0x06,
    ECLM_RES_O_API_GENERIC_FAILURE = 0x07,
    ECLM_RES_O_TIMEOUT             = 0xff   // not part of spec
};


/*
 * INTERNAL STRUCTS
 */
 
// for ECLM_O_EE_AGE_RESP
typedef struct {
    UINT8 prnNum;
    EclmEphPos ephPos;
    UINT16 eePosAge;
    UINT16 cgeePosGPSWeek;
    UINT16 cgeePosTOE;
    UINT8 ephClkFlag;
    UINT16 eeClkAge;
    UINT16 cgeeClkGPSWeek;
    UINT16 cgeeClkTOE;
} EclmEeAge;


/*
 * MESSAGE STRUCTS
 */
 
// ECLM_I_START_DL
// no data

// ECLM_I_FILE_SIZE
typedef struct {
    UINT32 fileLength;
} EclmSgeeDownloadFileSize;

// ECLM_I_PACKET_DATA
typedef struct {
    UINT16 seqNum;
    UINT16 dataLen;
    UINT8* data;
} EclmSgeeDownloadFileContent;

// ECLM_I_EE_AGE_REQ
// TODO

// ECLM_I_SGEE_AGE_REQ
typedef struct {
    UINT8 satPrn;
} EclmSgeeAgeRequest;

// ECLM_I_HOST_FILE
typedef struct {
    UINT16 seqNum;
    EclmStorageId nvmId;
    UINT8 blockCount;
    UINT16* blockSizes;
    UINT32* blockOffsets;
    UINT8* data;
} EclmHostFileContent;

// ECLM_I_ACK_NACK / ECLM_O_ACK_NACK
typedef struct {
    UINT8 eclmId;
    EclmAckNack ackNack;
    EclmResultOut result;
} EclmAckNackResponse;

// ECLM_O_EE_AGE_RESP
typedef struct {
    UINT8 count;
    EclmEeAge* satAges;
} EclmEeAgeResponse;

// ECLM_O_SGEE_AGE_RESP
typedef struct {
    UINT32 age;
    UINT32 interval;
} EclmSgeeAgeResponse;

// ECLM_O_SGEE_REQ
typedef struct {
    UINT8 start;
    UINT32 delay;
} EclmSgeeRequest;
 
// ECLM_O_FILE_ERASE
typedef struct {
    EclmStorageId nvmId;
} EclmFileErase;

// ECLM_O_FILE_UPDATE
typedef struct {
    EclmStorageId nvmId;
    UINT16 size;
    UINT32 offset;
    UINT16 seqNum;
    UINT8* data;
} EclmFileUpdate;

// ECLM_O_FILE_REQ
typedef struct {
    EclmStorageId nvmId;
    UINT16 seqNum;
    UINT8 blockCount;
    UINT16* blockSizes;
    UINT32* blockOffsets;
} EclmFileRequest;

#endif /* _ECLM_TYPES_H_ */
