/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _PATCH_FILE_H_
#define _PATCH_FILE_H_

#include "patch_manager_types.h"


#if DBG_PATCH
#define DEBUG_PATCH(...)            DEBUG(__VA_ARGS__)
#else
#define DEBUG_PATCH(...)            ((void)0)
#endif /* DBG_PATCH */


#define PM_MAX_BUFFER_SIZE  1024

/* 0xA0, 0xA2, 2 bytes Payload Length, 2 byte for Checksum, 0xB0, and 0xB3 */
#define PM_ENVELOPE_SIZE    8

/* 1024 - 8 = 1016, removes A0A2, MsgSize, and B0B3 */
#define PM_MAX_INPUT_PAYLOAD_SIZE (PM_MAX_BUFFER_SIZE - PM_ENVELOPE_SIZE)

/* 1016 - 4 = 1012, removes MID, SID, and Seqno */
#define PM_MAX_LOADDATA_SIZE (PM_MAX_INPUT_PAYLOAD_SIZE - 4)

/* 1016 - 18 = 998, removes initial packet header */
#define PM_MAX_LOADDATAINIT_SIZE (PM_MAX_INPUT_PAYLOAD_SIZE - 18)

#define PM_CHECKSUM_MASK  0x7FFF

// Current estimate is around 4.2 seconds for chip to respond
// after the patch file load exit request is sent.
// Add a little extra time just incase.
#define PM_TIMEOUT_MS        6000

#define PM_HEADER_SIZE          16

/* 4e ROM versions since 0x1A use CRC32 instead of CRC16 */
#define ROMVERSIONCODE_USE_CRC32  0x1A


typedef struct {
   UINT16 chipId;
   UINT16 siliconVersion;
   UINT16 romVersionCode;
   UINT16 patchRevisionCode;
   UINT32 patchDataBaseAddress;
   UINT16 patchDataLength;
} PatchFileHeader;

typedef struct {
    UINT8 crcSize;    // in bytes, 2 (16-bit) or 4 (32-bit)
    UINT32 crcValue;
    UINT16 patchRamStartOffset;
    UINT32 overlayCrc;
    UINT32 nonOverlayCrc;
} PatchFileInfo;

typedef struct {
    UINT16 offset;
    UINT16 seqNum;
} PatchFileState;

typedef struct {
    PatchFileHeader header;
    PatchFileInfo info;
    PatchFileState state;
    UINT8* data;
} PatchFile;

PatchFile* patch_file_init( const char* path );

void patch_file_free( PatchFile* pf );
    
EXIT_CODE patch_file_validate( PatchFile* pf, PatchManagerPrompt* prompt );

EXIT_CODE patch_file_get_next_request( PatchFile* pf, PatchManagerLoadRequest* reqObj, UINT8* finalPkt );


#endif /* _PATCH_FILE_H_ */

