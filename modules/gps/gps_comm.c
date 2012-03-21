/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * TTY functions
 *
 */


#include "gps_comm.h"

EXIT_CODE gps_comm_write( GpsComm* c, UINT8* buf, UINT16 len )
{
    int count, idx = 0;
    
    while (idx < len) {    
        count = write(c->fd, buf + idx, len - idx);
        
        if (count < 0) {
            if (errno == EINTR) continue;
            ERROR("error writing (%d) %s: %s", errno, strerror(errno), buf);
            return EXIT_FAILURE;
        }
        
        fsync(c->fd);
        idx += count;
    }

    return EXIT_SUCCESS;
}

EXIT_CODE gps_comm_cfg( GpsComm* c, GpsBaudRate rate )
{
    struct termios tio;
    speed_t speed;
    
    switch ( rate ) {
        case GPS_BAUD_1200:
            speed = B1200;
            break;
            
        case GPS_BAUD_2400:
            speed = B2400;
            break;
            
        case GPS_BAUD_4800:
            speed = B4800;
            break;
            
        case GPS_BAUD_9600:
            speed = B9600;
            break;
            
        case GPS_BAUD_38400:
            speed = B38400;
            break;
            
        case GPS_BAUD_57600:
            speed = B57600;
            break;
            
        case GPS_BAUD_115200:
            speed = B115200;
            break;
            
        default:
            ERROR("invalid rate: %d", rate);
            return EXIT_FAILURE;
    }
    
    tio.c_cflag = CLOCAL | CREAD | CS8;
    tio.c_iflag = IGNPAR;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    
    if ( cfsetispeed(&tio, speed) != 0 ) {
        ERROR("failed at cfsetispeed: (%d) %s", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    
    if ( cfsetospeed(&tio, speed) != 0 ) {
        ERROR("failed at cfsetospeed: (%d) %s", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    
    if ( tcsetattr(c->fd, TCSANOW, &tio) != 0 ) {
        ERROR("failed at tcsetattr: (%d) %s", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    
    tcflush(c->fd, TCIOFLUSH);
    
    c->rate = rate;
    
    return EXIT_SUCCESS;
}

EXIT_CODE gps_comm_open( GpsComm* c, const char* device )
{
    struct termios tio;
    DEBUG("device=%s", device);
    
    if ((c->fd = open(device, O_RDWR)) < 0) {
        ERROR("failed to open %s: (%d) %s", device, errno, strerror(errno));
        return EXIT_FAILURE;
    }
    
    if (FAILED( gps_comm_cfg( c, GPS_BAUD_4800 ) )) {
        close(c->fd);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
