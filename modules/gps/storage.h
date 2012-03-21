/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */
 
#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "gps.h"
#include <sys/stat.h>

EXIT_CODE storage_exists( 
    const char *path 
); 

EXIT_CODE storage_erase( 
    const char *path 
);

EXIT_CODE storage_write( 
    const char *path,
    size_t size,
    off_t offset,
    void* data
);

int storage_read( 
    const char *path,
    size_t size,
    off_t offset,
    void* data
);

#endif /* _STORAGE_H_ */
