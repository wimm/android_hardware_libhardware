/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#include "patch_file.h"

#include "buffer.h"
#include "crc.h"


EXIT_CODE patch_file_get_next_request( 
    PatchFile* pf, 
    PatchManagerLoadRequest* reqObj,
    UINT8* finalPkt
) {
    int i, appendCrc = 0, remainingData;
    UINT16 readCount = 0;
    UINT32 crcValue = 0;

    Buffer b;
    UINT8 buff[PM_MAX_BUFFER_SIZE];
    b.data = buff;
    b.pos = 0;

    // start with seq = 1
    pf->state.seqNum++;
    
    // initial packet
    if ( pf->state.seqNum == 1 ) {
        b.len = PM_MAX_LOADDATAINIT_SIZE;
        
        reqObj->seqNumber = 1;
        reqObj->romVersion = pf->header.romVersionCode;
        reqObj->patchVersion = pf->header.patchRevisionCode;
        reqObj->patchDataBaseAddr = pf->header.patchDataBaseAddress;
        
        /* Add CRC size to patch data length */
        /* Make sure 4e remembers that the patch data length is crcSize extra bytes, so */
        /* actual patch data is - crcSize bytes */
        reqObj->patchDataLength = pf->header.patchDataLength + pf->info.crcSize;
        
        /* Add CRC size to PatchRAMStartOffset only if PatchRAMStartOffset is > 0 */
        /* Make sure 4e remembers that if the PatchRAMStartOffset is > 0 that it  */
        /* contains gPatchFileCRCSize extra bytes for the overlay CRC.            */
        if ( pf->info.patchRamStartOffset > 0 ) {
            reqObj->patchDataRamOffset = pf->info.patchRamStartOffset + pf->info.crcSize;
        }
    
        /* This code sends overlay section in the first segment and then sends the */
        /* new patch data segments in subsequent segment(s).                       */
        /* Compute state fields for patch file, which may be zero length */
        /* Set 1st load message length based on the minimum between the load */
        /* message size or how much patch data is left.   */
        if ( pf->info.patchRamStartOffset > 0 ) {
            readCount = MIN(
                PM_MAX_LOADDATAINIT_SIZE - pf->info.crcSize, 
                pf->info.patchRamStartOffset
            );
        } else {
            readCount = MIN(
                PM_MAX_LOADDATAINIT_SIZE - pf->info.crcSize, 
                pf->header.patchDataLength
            );
        }
        
    } else {
        b.len = PM_MAX_LOADDATA_SIZE;
        
        reqObj->seqNumber = pf->state.seqNum;

        readCount = pf->header.patchDataLength - pf->state.offset;
        if ( readCount > PM_MAX_LOADDATA_SIZE ) {
            readCount = PM_MAX_LOADDATA_SIZE;
        } else if ( readCount + pf->info.crcSize > PM_MAX_LOADDATA_SIZE ) {
            ERROR("last packet + crc would overflow data: need to handle this case!");
            return EXIT_FAILURE;
        }
    }

    /* Decided not to allow a zero length patch to be sent. Previously for 3tw */
    /* a zero length patch could be sent to delete current patch. */
    if (readCount == 0) {
        ERROR("will not send zero length patch");
        return EXIT_FAILURE;
    }

    if ( pf->state.offset < pf->info.patchRamStartOffset ) {
        DEBUG_PATCH("packet %d: reading %d at offset %d of %d total bytes of overlay patch data",
            pf->state.seqNum, readCount, pf->state.offset, pf->info.patchRamStartOffset);
    } else {
        DEBUG_PATCH("packet %d: reading %d at offset %d of %d total bytes of non-overlay patch data",
            pf->state.seqNum, readCount, pf->state.offset, pf->header.patchDataLength - pf->info.patchRamStartOffset);
    }

    /* Copy patch data */
    if (FAILED( write_data( &b, pf->data + pf->state.offset, readCount ) )) {
        ERROR("failed to copy data");
        return EXIT_FAILURE;
    }
    pf->state.offset += readCount;

    remainingData = ( pf->header.patchDataLength - pf->state.offset );
    
    DEBUG_PATCH("patchLength=%d offset=%d remainingData=%d", 
        pf->header.patchDataLength, pf->state.offset, remainingData);
    
    /* If final load segment, then don't forget to add patch CRC */
    if ( remainingData == 0 ) {
        if ( finalPkt ) *finalPkt = 1;

        // determine CRC to append
        appendCrc = 1;
        if ( pf->info.patchRamStartOffset > 0 ) {
            crcValue = pf->info.nonOverlayCrc;
            DEBUG_PATCH("last packet: appending non-overlay CRC: 0x%08x", crcValue);
        } else {
            crcValue = pf->info.overlayCrc;
            DEBUG_PATCH("last packet: appending overlay CRC: 0x%08x", crcValue);
        }
        
    } else if ( pf->state.seqNum == 1 ) {

        /* Calculate overlay CRC for 1st load message only */
        appendCrc = 1;
        crcValue = pf->info.overlayCrc;
        DEBUG_PATCH("first packet: appending overlay CRC: 0x%08x", crcValue);
    }
    
    if ( appendCrc ) {
    
        /* PatchRevisionCode of 255 will be used to delete existing patch. */
        if ( pf->header.patchRevisionCode == 0xff ) {
            crcValue = ~(crcValue);
        }
        
        if (FAILED( WRITE_INT( &b, crcValue, pf->info.crcSize ) )) {
            ERROR("failed write CRC");
            return EXIT_FAILURE;
        }
    }

    // copy data to request
    if ( b.pos > 0 ) {
        reqObj->data = malloc( b.pos );
        reqObj->dataLen = b.pos;
        memcpy( reqObj->data, b.data, b.pos );
    }
    
    return EXIT_SUCCESS;
}


EXIT_CODE patch_file_validate( PatchFile* pf, PatchManagerPrompt* prompt )
{
    DEBUG_PATCH("chipId       = 0x%04x", prompt->chipId);
    DEBUG_PATCH("siliconId    = 0x%04x", prompt->siliconId);
    DEBUG_PATCH("romVersion   = 0x%04x", prompt->romVersion);
    DEBUG_PATCH("patchVersion = 0x%04x", prompt->patchVersion);
    
    if ( pf->header.chipId != prompt->chipId ) {
        ERROR("chip mismatch: 0x%04x != 0x%04x", pf->header.chipId, prompt->chipId);
        return EXIT_FAILURE;
    }
    
    if ( pf->header.siliconVersion != prompt->siliconId ) {
        ERROR("silicon mismatch: 0x%04x != 0x%04x", pf->header.siliconVersion, prompt->siliconId);
        return EXIT_FAILURE;
    }
    
    if ( pf->header.romVersionCode != prompt->romVersion ) {
        ERROR("ROM mismatch: 0x%04x != 0x%04x", pf->header.romVersionCode, prompt->romVersion);
        return EXIT_FAILURE;
    }
    
    if ( pf->header.patchRevisionCode < prompt->patchVersion ) {
        ERROR("version mismatch: 0x%04x < 0x%04x", pf->header.patchRevisionCode, prompt->patchVersion);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}


EXIT_CODE _patch_file_read_header( PatchFile* pf, int fd )
{
    Buffer br;
    UINT8 buf[PM_HEADER_SIZE];
    ssize_t readRet;
    
    UINT8 byte1, byte2;
    
    readRet = read( fd, buf, PM_HEADER_SIZE );
    if ( readRet < 0 ) {
        ERROR("error at read: errno=%d %s", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    if ( readRet < (ssize_t)sizeof( buf ) ) {
        ERROR("read under-run: %ld < %d", readRet, PM_HEADER_SIZE);
        return EXIT_FAILURE;
    }
    
    br = (Buffer){ buf, readRet, 0 };
    
    FAIL_UNLESS( READ_VAR( &br, byte1 ) );
    FAIL_UNLESS( READ_VAR( &br, byte2 ) );
    
    if ( ( byte1 != 'P' ) || ( byte2 != 'D' ) ) {
        ERROR("bad start bytes... not a patch file?");
        return EXIT_FAILURE;
    }
    
    FAIL_UNLESS( READ_VAR( &br, pf->header.chipId ) );
    FAIL_UNLESS( READ_VAR( &br, pf->header.siliconVersion ) );
    FAIL_UNLESS( READ_VAR( &br, pf->header.romVersionCode ) );
    FAIL_UNLESS( READ_VAR( &br, pf->header.patchRevisionCode ) );
    FAIL_UNLESS( READ_VAR( &br, pf->header.patchDataBaseAddress ) );
    FAIL_UNLESS( READ_VAR( &br, pf->header.patchDataLength ) );
    
    DEBUG_PATCH("chipId       = 0x%04x", pf->header.chipId);
    DEBUG_PATCH("siliconId    = 0x%04x", pf->header.siliconVersion);
    DEBUG_PATCH("romVersion   = 0x%04x", pf->header.romVersionCode);
    DEBUG_PATCH("patchVersion = 0x%04x", pf->header.patchRevisionCode);
    DEBUG_PATCH("baseAddress  = 0x%08x", pf->header.patchDataBaseAddress);
    DEBUG_PATCH("dataLength   = 0x%04x", pf->header.patchDataLength);
    
    return EXIT_SUCCESS;
}

EXIT_CODE _patch_file_read_data( PatchFile* pf, int fd )
{
    ssize_t readRet;
    UINT16 length = pf->header.patchDataLength;
    
    // no data (valid?)
    if ( length == 0 ) return EXIT_SUCCESS;
    
    pf->data = malloc( length );
    
    readRet = read( fd, pf->data, length );
    if ( readRet < 0 ) {
        ERROR("error at read: errno=%d %s", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    if ( readRet < length ) {
        ERROR("read under-run: %ld < %d", readRet, length);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

EXIT_CODE _patch_file_read_crc( PatchFile* pf, int fd )
{
    Buffer br;
    UINT8 crcBuf[4];
    ssize_t readRet;
    
    UINT32 crcActual;
    
    pf->info.crcSize = ( pf->header.romVersionCode >= ROMVERSIONCODE_USE_CRC32 ) ? 4 : 2;
    
    readRet = read( fd, crcBuf, pf->info.crcSize );
    if ( readRet < 0 ) {
        ERROR("error at read: errno=%d %s", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    if ( readRet < pf->info.crcSize ) {
        ERROR("read under-run: %ld < %d", readRet, pf->info.crcSize);
        return EXIT_FAILURE;
    }
    
    br = (Buffer){ crcBuf, readRet, 0 };
    FAIL_UNLESS( READ_INT( &br, pf->info.crcValue, pf->info.crcSize ) );
    
    if ( pf->info.crcSize == 4 ) {
        crcActual = CRC32(pf->data, pf->header.patchDataLength, 0xFFFFFFFF);
    } else { 
        crcActual = CRC16(pf->data, pf->header.patchDataLength, 0xFFFF);
    }
    
    if ( pf->info.crcValue != crcActual ) {
        ERROR("CRC check failed: 0x%08x != 0x%08x", pf->info.crcValue, crcActual);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}


EXIT_CODE _patch_file_find_offsets( PatchFile* pf )
{
    Buffer br;
    
    UINT8 overtype;
    UINT16 seglen;
    UINT32 segoff;
    
    UINT16 offset;
    UINT32 overlaysect = 0;
    UINT8 nonoverlay_flag = 0;
    
    br = (Buffer){ pf->data, pf->header.patchDataLength, 0 };
        
    /* Process patch payload data until cur equal len */
    while (br.pos < br.len) {
        offset = br.pos;
        FAIL_UNLESS( READ_VAR( &br, segoff ) );
        
        /* Check if segment offset (addr) is an overlay. */
        if (segoff < 0x200000) {
            /* Overlay section, so 1st byte is overlay type and 2nd byte is segment length. */
            FAIL_UNLESS( READ_INT( &br, overtype, 1 ) );
            FAIL_UNLESS( READ_INT( &br, seglen, 1 ) );
            overlaysect++;
        } else {
            /* For new patch function code, segment length is 2 bytes like before */
            FAIL_UNLESS( READ_INT( &br, seglen, 2 ) );
            pf->info.patchRamStartOffset = offset;
            nonoverlay_flag = 1;
        }
        
        br.pos += seglen;
    }
   
    DEBUG_PATCH("Start offset: 0x%08x", pf->info.patchRamStartOffset);
    
    /* If overlay sections found, then process them. */
    if (overlaysect > 0)
    {
        /* Calculate CRC for patch overlay section only */
        if (pf->info.patchRamStartOffset > 0)
        {
            if ( pf->info.crcSize == 4 ) {
                pf->info.overlayCrc = CRC32(pf->data, pf->info.patchRamStartOffset, 0xFFFFFFFF);
            } else { 
                pf->info.overlayCrc = CRC16(pf->data, pf->info.patchRamStartOffset, 0xFFFF);
            }
        } else {
            pf->info.overlayCrc = pf->info.crcValue;
        }
        
        DEBUG_PATCH("Overlay length: %d", pf->info.patchRamStartOffset);
        DEBUG_PATCH("Overlay CRC: 0x%08x", pf->info.overlayCrc);
    }
    
    /* If Non-Overlay section found, then compute its CRC without its segment address and segment length. */
    if (nonoverlay_flag)
    {
        /* Calculate CRC for patch nonoverlay section */
        /* Needed to add 6 to not perform CRC on offset and byte size for nonoverlay section */        
        UINT8 *nonOverlayPtr = pf->data + ( pf->info.patchRamStartOffset + 6 );
        int nonOverlaySize = pf->header.patchDataLength - ( pf->info.patchRamStartOffset + 6 );
        
        if ( pf->info.crcSize == 4 ) {
            pf->info.nonOverlayCrc = CRC32(nonOverlayPtr, nonOverlaySize, 0xFFFFFFFF);
        } else { 
            pf->info.nonOverlayCrc = CRC16(nonOverlayPtr, nonOverlaySize, 0xFFFF);
        }
        
        DEBUG_PATCH("Non-overlay length: %d", nonOverlaySize);
        DEBUG_PATCH("Non-overlay CRC: 0x%08x", pf->info.nonOverlayCrc);
   }

   return EXIT_SUCCESS;
}

EXIT_CODE _patch_file_load( PatchFile* pf, const char* path )
{
    int fd;
    EXIT_CODE res = EXIT_FAILURE;
    
    // open file
    fd = open(path, O_RDONLY);
    if ( fd < 0 ) {
        ERROR("error at open: path=%s errno=%d %s", path, errno, strerror(errno));
        return EXIT_FAILURE;
    }
    
    // read file
    if (FAILED( _patch_file_read_header( pf, fd ) )) goto end;
    if (FAILED( _patch_file_read_data( pf, fd ) )) goto end;
    if (FAILED( _patch_file_read_crc( pf, fd ) )) goto end;
    if (FAILED( _patch_file_find_offsets( pf ) )) goto end;
    
    res = EXIT_SUCCESS;
    
end:
    close( fd );
    return res;
}

PatchFile* patch_file_init( const char* path )
{
    PatchFile* pf = (PatchFile*) calloc( 1, sizeof( PatchFile ) );
    
    if (FAILED( _patch_file_load( pf, path ) )) {
        free( pf );
        return NULL;
    }
    
    return pf;
}
    
void patch_file_free( PatchFile* pf )
{
    if ( pf->data ) free( pf->data );
    free( pf );
}

