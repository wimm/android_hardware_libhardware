/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _PATCH_MANAGER_TYPES_H_
#define _PATCH_MANAGER_TYPES_H_

#include "osp_types.h"

typedef UINT8 PatchManagerAckStatus;
enum _PatchManagerAckStatus {
    PATCH_MANAGER_ACK                    = 0x3,
    PATCH_MANAGER_NACK                   = 0x2
};

// OSP_ID_PM_LOAD_REQ
typedef struct {
    UINT16 seqNumber;
    UINT16 romVersion;
    UINT16 patchVersion;
    UINT32 patchDataBaseAddr;
    UINT16 patchDataLength;
    UINT16 patchDataRamOffset;
    UINT16 dataLen;
    void* data;
} PatchManagerLoadRequest;

// OSP_ID_PM_EXIT_REQ
// no data

// OSP_ID_PM_START_REQ
// no data

// OSP_ID_PM_PROMPT
typedef struct {
    UINT16 chipId;
    UINT16 siliconId;
    UINT16 romVersion;
    UINT16 patchVersion;
} PatchManagerPrompt;

// OSP_ID_PM_ACK
typedef struct {
    UINT16 seqNumber;
    OspMinorId minorId;
    PatchManagerAckStatus status;
} PatchManagerAck;

#endif /* _PATCH_MANAGER_TYPES_H_ */
