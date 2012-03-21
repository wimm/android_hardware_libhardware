/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _WORK_THREAD_ECLM_H_
#define _WORK_THREAD_ECLM_H_

#include "eclm_types.h"
#include "work_thread.h"

EclmResultIn eclm_storage_erase( 
    EclmStorageId id 
);

EXIT_CODE eclm_send_sgee_start_download(
    WorkState* s,
    EclmResultOut* result
);

EXIT_CODE eclm_send_sgee_file_size(
    WorkState* s,
    UINT32 fileLength,
    EclmResultOut* result
);

EXIT_CODE eclm_send_sgee_file_content(
    WorkState* s,
    EclmSgeeDownloadFileContent* obj,
    EclmResultOut* result
);

EXIT_CODE eclm_send_sgee_age_request(
    WorkState* s,
    UINT8 satPrn,
    EclmResultOut* result
);

void eclm_handle_msg( WorkThread* t, Message *msg );

#endif /* _WORK_THREAD_ECLM_H_ */
