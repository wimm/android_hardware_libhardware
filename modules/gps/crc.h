/*    This Software is protected by United States copyright laws and       *
*    international treaties.  You may not reverse engineer, decompile     *
*    or disassemble this Software.                                        *
*                                                                         *
*    WARNING:                                                             *
*    This Software contains SiRF Technology Inc.’s confidential and       *
*    proprietary information. UNAUTHORIZED COPYING, USE, DISTRIBUTION,    *
*    PUBLICATION, TRANSFER, SALE, RENTAL OR DISCLOSURE IS PROHIBITED      *
*    AND MAY RESULT IN SERIOUS LEGAL CONSEQUENCES.  Do not copy this      *
*    Software without SiRF Technology, Inc.’s  express written            *
*    permission.   Use of any portion of the contents of this Software    *
*    is subject to and restricted by your signed written agreement with   *
*    SiRF Technology, Inc.                                                *
*                                                                         *
***************************************************************************
*
* MODULE:  ROM Patch Appllication
*
* FILENAME:  crc.h
*
* DESCRIPTION: CRC Calculation related function declaration
*
***************************************************************************
*
*  Keywords for Perforce.  Do not modify.
*
*  $File: //core/tools/ROMPatchApplication/src/crc.h $
*
*  $DateTime: 2011/07/05 18:52:25 $
*
*  $Revision: #2 $
*
***************************************************************************/

#ifndef  __CRC_H_
#define  __CRC_H_

#include "gps.h"

extern UINT16 CRC16(UINT8 *pData, UINT32 len, UINT16 uCRC);
extern UINT32 CRC32(UINT8 *pData, UINT32 len, UINT32 uCRC);

#endif /* __CRC_H_*/
