/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Modified from sdk/emulator/gps/gps_qemu.c
 */


#include "read_thread.h"

#include "nmea_parser.h"
#include "osp_parser.h"

#include <sys/epoll.h>
#include <sys/socket.h>

enum {
    CMD_QUIT  = 'q'
};

static int epoll_register( int  epoll_fd, int  fd )
{
    struct epoll_event  ev;
    int                 ret, flags;

    /* important: make the fd non-blocking */
    flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    do {
        ret = epoll_ctl( epoll_fd, EPOLL_CTL_ADD, fd, &ev );
    } while (ret < 0 && errno == EINTR);
    return ret;
}

static int epoll_deregister( int  epoll_fd, int  fd )
{
    int  ret;
    do {
        ret = epoll_ctl( epoll_fd, EPOLL_CTL_DEL, fd, NULL );
    } while (ret < 0 && errno == EINTR);
    return ret;
}

static int _read_thread_cmd( ReadThread* t, char cmd )
{
    int ret;
    
    if ( t->control[0] == -1 ) {
        ERROR("socket not present");
        return EXIT_FAILURE;
    }

    do { ret=write( t->control[0], &cmd, 1 ); }
    while (ret < 0 && errno == EINTR);

    if (ret != 1) {
        ERROR("could not send command %c: ret=%d: (%d) %s", 
            cmd, ret, errno, strerror(errno));
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

static void read_thread( void* arg )
{
    ReadThread* t = (ReadThread*)arg;
    int gracefulExit = 0;
    
    NmeaParser*             nmeaParser;
    OspParser*              ospParser;
    
    int         epoll_fd   = epoll_create(2);
    int         gps_fd     = t->gpsState->comm.fd;
    int         control_fd = t->control[1];

    // register control file descriptors for polling
    epoll_register( epoll_fd, control_fd );
    epoll_register( epoll_fd, gps_fd );

    ospParser = osp_parser_init( t->msgQueue );
    nmeaParser = nmea_parser_init( t->msgQueue, &t->gpsState->callbacks );
    
    INFO("starting");
    
    gps_state_set_status( t->gpsState, GPS_STATUS_ENGINE_ON );
    gps_state_dispatch( t->gpsState, GPS_DISPATCH_STATUS );
    
    // wait until after we dispatch the state change before we unblock our calling thread
    // this way we don't create a race condition between the read and work threads
    thread_enter( t->thread );
    
    // now loop
    for (;;) {
        int ne, nevents;
        struct epoll_event event[2];
        
        // get the next events
        nevents = epoll_wait( epoll_fd, event, 2, -1 );
        if (nevents < 0) {
            if (errno != EINTR)
                ERROR("epoll_wait() unexpected error: %s", strerror(errno));
            continue;
        }
        
        // DEBUG("received %d events", nevents);
        for (ne = 0; ne < nevents; ne++) {
            
            if ((event[ne].events & EPOLLHUP) != 0) {
                ERROR("epoll_wait() EPOLLHUP: %s", strerror(errno));
                goto exit;
            }
            
            if ((event[ne].events & EPOLLERR) != 0) {
                ERROR("epoll_wait() EPOLLERR: %s", strerror(errno));
                goto exit;
            }
            
            if ((event[ne].events & EPOLLIN) != 0) {
                int  fd = event[ne].data.fd;
    
                if (fd == control_fd) {
                    int   ret;
                    char  cmd = 255;
                    do {
                        ret = read( fd, &cmd, 1 );
                    } while (ret < 0 && errno == EINTR);
                    
                    DEBUG("cmd=%c", cmd);
                    switch (cmd) {
                    
                        case CMD_QUIT:
                            gracefulExit = 1;
                            goto exit;
                        
                        default:
                            ERROR("unknown command %c", cmd);
                    }
                }
                
                else if (fd == gps_fd) {
                    int ret;
                    UINT8 buff[32];
                
                    // TODO: move reading to GpsComm
                    while (1)
                    {   
                        ret = read( t->gpsState->comm.fd, buff, sizeof(buff) );
                        if (ret < 0) {
                            // interrupted
                            if (errno == EINTR) continue;
                            
                            // no data
                            if (errno == EAGAIN) break;
                            
                            ERROR("error reading: (%d) %s:", errno, strerror(errno));
                            break;
                        }
                        
                //        DEBUG("received %d bytes:", ret);
                //        PRINT_DATA( buff, ret );
                
                        if ( gps_state_get_status( t->gpsState ) != GPS_STATUS_SESSION_BEGIN ) {
                            WARN("not in active mode, discarding %d bytes:", ret);
                            // PRINT_DATA( buff, ret );
                        } else {
                            switch ( gps_state_get_protocol( t->gpsState ) ) 
                            {
                                case GPS_PROTO_NMEA:
                                    nmea_parser_read( nmeaParser, buff, ret );
                                    break;
                                    
                                case GPS_PROTO_OSP:
                                    osp_parser_read( ospParser, buff, ret );
                                    break;
                                    
                                default: 
                                    ERROR("unknown protocol");
                                    break;
                            }
                        }
                    }
                } else {
                    ERROR("unknown fd=%d", fd);
                }
            }
        }
    }
    
exit:
    INFO("exiting (active=%d)", t->active);
    
    gps_state_set_status( t->gpsState, GPS_STATUS_ENGINE_OFF );
    gps_state_dispatch( t->gpsState, GPS_DISPATCH_STATUS );
    
    nmea_parser_free( nmeaParser );
    osp_parser_free( ospParser );

    epoll_deregister( epoll_fd, control_fd );
    epoll_deregister( epoll_fd, gps_fd );
    
    if ( t->active ) {
        t->active = 0;
        thread_free( t->thread );
        t->thread = NULL;
        
        close( t->control[0] );
        close( t->control[1] );
    } else {
        thread_exit( t->thread );
    }
    
    INFO("exited");
}


/*
 * THREAD CONTROL
 */

EXIT_CODE read_thread_start( ReadThread* t ) 
{
    if ( t->thread ) {
        WARN("thread already running");
        return EXIT_SUCCESS;
    }
    
    INFO("");
    
    if ( socketpair( AF_LOCAL, SOCK_STREAM, 0, t->control ) < 0 ) {
        ERROR("could not create socket pair: (%d) %s", errno, strerror(errno));
        return EXIT_FAILURE;
    }

    t->active = 1;
    t->thread = gps_state_create_thread( t->gpsState, "gps_read_thread", read_thread, t );
    if ( !t->thread ) {
        t->active = 0;
    
        // close the control socket pair
        close( t->control[0] );
        close( t->control[1] );
    
        return EXIT_FAILURE;
    }
    
    thread_wait( t->thread );
    return EXIT_SUCCESS;
}

EXIT_CODE read_thread_stop( ReadThread* t )
{
    if ( !t->thread ) {
        WARN("thread not running");
        return EXIT_SUCCESS;
    }
    
    // wait for thread to quit
    INFO("waiting for thread to exit");
    t->active = 0;
    _read_thread_cmd( t, CMD_QUIT );
    thread_join( t->thread );
    thread_free( t->thread );
    t->thread = NULL;
    INFO("thread exited");
    
    // close the control socket pair
    close( t->control[0] );
    close( t->control[1] );

    return EXIT_SUCCESS;
}

ReadThread* read_thread_init( GpsState* s ) {
    ReadThread* t = (ReadThread*) calloc( 1, sizeof( ReadThread ) );
    
    t->gpsState = s;
    t->msgQueue = message_queue_init();
    
    return t;
}

void read_thread_free( ReadThread* t )
{
    if ( !t ) return;
    
    if ( t->thread )
        ERROR("thread freed before self exited!");
    
    message_queue_free( t->msgQueue );
    
    free( t );
}
