/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * Non-Volatile Memory functions
 *
 */


#include "storage.h"

EXIT_CODE storage_exists( const char *path ) 
{
    struct stat unused;   
    
    if ( stat( path, &unused ) != 0 ) 
        return EXIT_FAILURE;
        
    return EXIT_SUCCESS;
}

EXIT_CODE storage_erase( const char *path ) 
{
    if ( !path )
        return EXIT_FAILURE;
        
    if ( remove( path ) ) {
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

EXIT_CODE storage_write( 
    const char *path,
    size_t size,
    off_t offset,
    void* data
) {
    int fd = -1;
    int writeLen = -1;
    struct stat stats;
    EXIT_CODE result = EXIT_FAILURE;
    
    if ( !path ) {
        goto end;
    }
    
    // open file
    fd = open(path, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR );
    if ( fd < 0 ) {
        ERROR("error at open: path=%s size=%ud offset=%ld errno=%d %s", 
            path, size, offset, errno, strerror(errno));
        goto end;
    }
    
    // get size
    if ( fstat( fd, &stats ) != 0 ) {
        ERROR("error at fstat: path=%s size=%ud offset=%ld errno=%d %s", 
            path, size, offset, errno, strerror(errno));
        goto end;
    }
    
    // see if we need to add some padding to get to the offset
    if ( offset > stats.st_size ) {
        int padLen = offset - stats.st_size;
        void *padBuf = malloc( padLen );
        memset( padBuf, 0, padLen );
        
        DEBUG("adding %d bytes of padding", padLen);
        writeLen = write(fd, padBuf, padLen);
        if ( writeLen < 0 ) {
            ERROR("error at write padding: path=%s size=%ud offset=%ld errno=%d %s", 
                path, size, offset, errno, strerror(errno));
            free( padBuf );
            goto end;
        }
        free( padBuf );
    }
    
    // seek to offset
    if ( lseek(fd, offset, SEEK_SET) < 0 ) {
        ERROR("error at lseek: path=%s size=%ud offset=%ld errno=%d %s", 
            path, size, offset, errno, strerror(errno));
        goto end;
    }
    
    // write data
    writeLen = write(fd, data, size);
    if ( writeLen < 0 ) {
        ERROR("error at write: path=%s size=%ud offset=%ld errno=%d %s", 
            path, size, offset, errno, strerror(errno));
        goto end;
    }
    
    result = EXIT_SUCCESS;

end:
    if (fd >=0) close(fd);
    return result;
}

int storage_read( 
    const char *path,
    size_t size,
    off_t offset,
    void* data
) {
    int fd = -1;
    int readLen = -1;
    
    if ( !path ) goto end;
    
    fd = open(path, O_RDONLY);
    if ( fd < 0 ) {
        ERROR("error at open: path=%s size=%u offset=%ld errno=%d %s", 
            path, size, offset, errno, strerror(errno));
        goto end;
    }
    
    if ( lseek(fd, offset, SEEK_SET) < 0 ) {
        ERROR("error at lseek: path=%s size=%u offset=%ld errno=%d %s", 
            path, size, offset, errno, strerror(errno));
        goto end;
    }
    
    readLen = read(fd, data, size);
    if ( readLen < 0 ) {
        ERROR("error at read: path=%s size=%u offset=%ld errno=%d %s", 
            path, size, offset, errno, strerror(errno));
        goto end;
    }

end:
    if (fd >=0) close(fd);
    return readLen;
}



