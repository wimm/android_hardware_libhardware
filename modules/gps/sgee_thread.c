/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */

#include "sgee_thread.h"

#include "work_thread_eclm.h"
#include "work_thread_sirf.h"

#include "nmea_types.h"
#include "osp_types.h"

#include <sys/stat.h>

EXIT_CODE sgee_send_file( SgeeThread* t, char* path )
{
    int result = EXIT_FAILURE;

    int attempt;
    EclmResultOut res;
    EclmSgeeDownloadFileContent contentMsg;
    
    DEBUG("");
    
    int fd = -1;
    int bytesRead;
    int pktSeqNum = 0;
    struct stat stats;
    
    void* data;
    int maxDataSize;
    
    // clear local storage
    // eclm_storage_erase( ECLM_STORAGE_SGEE );
    
    if ( gps_state_get_protocol( t->workState.gpsState ) == GPS_PROTO_OSP ) {
        maxDataSize = MIN( GPS_SGEE_MAX_BUFFER_SIZE, OSP_DATA_SIZE - 6 );
    } else {
        maxDataSize = MIN( GPS_SGEE_MAX_BUFFER_SIZE, (NMEA_DATA_SIZE/3) - 1 );
    }
    
    data = malloc( maxDataSize );
    contentMsg.seqNum = 0;

    // open file
    fd = open(path, O_RDONLY);
    if ( fd < 0 ) {
        ERROR("error at open: path=%s errno=%d %s", path, errno, strerror(errno));
        goto end;
    }
    
    // get size
    if ( fstat( fd, &stats ) != 0 ) {
        ERROR("error at fstat: path=%s errno=%d %s", path, errno, strerror(errno));
        goto end;
    }

    // initiate download
    attempt = 0; 
    res = ECLM_RES_O_TIMEOUT;
    while ((attempt++ < 3) && (res == ECLM_RES_O_TIMEOUT)) {
        if (!t->workState.active) goto end;
        eclm_send_sgee_start_download( &t->workState, &res );
    }
    if (res != ECLM_RES_O_SUCCESS) {
        ERROR("failed to initiate download: attempt=%d res=0x%x", attempt, res);
        goto end;
    }
    
    // send file size
    attempt = 0; 
    res = ECLM_RES_O_TIMEOUT;
    while ((attempt++ < 3) && (res == ECLM_RES_O_TIMEOUT)) {
        if (!t->workState.active) goto end;
        eclm_send_sgee_file_size( &t->workState, stats.st_size, &res );
    }
    if (res != ECLM_RES_O_SUCCESS) {
        ERROR("failed to send file size: attempt=%d res=0x%x", attempt, res);
        goto end;
    }
    
    // send file data
    while (t->workState.active)
    {
        bytesRead = read(fd, data, maxDataSize);
        if ( bytesRead < 0 ) {
            ERROR("error at read: path=%s errno=%d %s", path, errno, strerror(errno));
            goto end;
        }
        if ( bytesRead == 0 ) {
            result = EXIT_SUCCESS;
            break;
        }
        
        contentMsg.seqNum++;
        
        attempt = 0;
        res = ECLM_RES_O_TIMEOUT;
        
        while ((attempt++ < 3) && (res == ECLM_RES_O_TIMEOUT)) {
            if (!t->workState.active) goto end;
            
            // malloc from inside attempt loop as message data will be freed
            // if message fails to send
            contentMsg.dataLen = bytesRead;
            contentMsg.data = malloc( bytesRead );
            memcpy( contentMsg.data, data, bytesRead );
            
            eclm_send_sgee_file_content( &t->workState, &contentMsg, &res );
        }
        
        if (res != ECLM_RES_O_SUCCESS) {
            ERROR("failed to send file data: 0x%x", res);
            goto end;
        }
    }
    
end:
    if (fd >=0) close(fd);
    free( data );
    return result;
}

static void sgee_thread( void* arg )
{
    SgeeThread* t = (SgeeThread*)arg;
    WorkState* s = &t->workState;
    
    GpsProtocol initialProto = gps_state_get_protocol( s->gpsState );
    GpsBaudRate initialRate = s->gpsState->comm.rate;
    INFO("initial protocol=%d rate=%d", initialProto, initialRate);
    
    INFO("starting");
    thread_enter( t->thread );
    
    // config comm
    if (FAILED( sirf_set_comm( s, GPS_SGEE_PROTO, GPS_SGEE_RATE ) ))
        goto end;
    
    // send file
    sgee_send_file( t, GPS_SGEE_PATH );
    
end:
    INFO("exiting (active=%d)", t->workState.active);
    
    // restore comm
    sirf_set_comm( s, initialProto, initialRate );
    
    // cleanup
    if ( t->workState.active ) {
        t->workState.active = 0;   
        thread_free( t->thread );
        t->thread = NULL;
    } else {
        thread_exit( t->thread );
    }
    
    INFO("exited");
}


EXIT_CODE sgee_thread_start( SgeeThread* t )
{
    if ( t->thread ) {
        WARN("thread already running");
        return EXIT_SUCCESS;
    }
    
    INFO("");
    
    t->workState.active = 1;
    t->thread = gps_state_create_thread( 
        t->workState.gpsState, "gps_sgee_thread", sgee_thread, t
    );
    
    if ( !t->thread ) {
        t->workState.active = 0;
        return EXIT_FAILURE;
    }
    
    // wait for thread to start
    thread_wait( t->thread );
    
    return EXIT_SUCCESS;
}

EXIT_CODE sgee_thread_stop( SgeeThread* t )
{
    if ( !t->thread ) {
        WARN("thread not running");
        return EXIT_SUCCESS;
    }
    
    t->workState.active = 0;
    
    // wait for thread to quit
    INFO("waiting for thread to exit");
    message_queue_signal( t->workState.readQueue );
    thread_join( t->thread );    
    thread_free( t->thread );
    t->thread = NULL;
    INFO("thread exited");
    
    return EXIT_SUCCESS;
}

SgeeThread* sgee_thread_init( WorkState* s ) {
    SgeeThread* t = (SgeeThread*) calloc( 1, sizeof( SgeeThread ) );
    t->workState = *s;
    return t;
}

void sgee_thread_free( SgeeThread* t )
{
    if ( !t ) return;
    if ( t->thread ) ERROR("thread freed before exited!");
    free( t );
}
