/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * GpsInterface accessed by 
 * frameworks/base/services/jni/com_android_server_location_GpsLocationProvider.cpp
 *
 * NOTE: 
 *  gps_init:      Called only once, either at boot or when GPS is enabled.  Store callbacks.
 *  gps_cleanup:   Called each time the GPS listener count goes to 0, or when GPS is disabled.  Power down GPS.
 *  gps_start:     Called when GPS listeners are registered and a fix is requested.  Put GPS into active mode.
 *  gps_stop:      Called when GPS listeners are registered, but no fix is needed.  Put GPS into hibernate mode.
 */
 
#include "gps_state.h"
#include "gps_datetime.h"

#include "power.h"

#include "read_thread.h"
#include "write_thread.h"
#include "work_thread.h"

static GpsState*    _gps_state = NULL;
static ReadThread*  _read_thread = NULL;
static WriteThread* _write_thread = NULL;
static WorkThread*  _work_thread = NULL;

static int gps_init(GpsCallbacks* callbacks)
{
    if (_gps_state == NULL) {
        DEBUG("");
        
        // init
        _gps_state = gps_state_init(callbacks);
        if ( _gps_state == NULL ) 
            return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

static void gps_cleanup()
{
    if ( _gps_state == NULL ) {
        ERROR("uninitialized");
        return;
    }
    
    DEBUG("");
    
    if ( _work_thread ) {
        work_thread_stop( _work_thread );
        work_thread_free( _work_thread );
        _work_thread = NULL;
    }
    
    if ( _write_thread ) {
        write_thread_stop( _write_thread );
        write_thread_free( _write_thread );
        _write_thread = NULL;
    }
    
    if ( _read_thread ) {
        read_thread_stop( _read_thread );
        read_thread_free( _read_thread );
        _read_thread = NULL;
    }

    set_gps_power(0);
}

static int gps_start()
{
    int hasSgee = 0;
    
    if ( _gps_state == NULL ) {
        ERROR("uninitialized");
        return EXIT_FAILURE;
    }

    if ( get_gps_power() == 0 ) {
        DEBUG("powering on");
        
        gps_state_set_flag( _gps_state, GPS_FLAG_HAS_PATCH, 0 );
        gps_state_set_protocol( _gps_state, GPS_PROTO_NMEA );
        
        if ( set_gps_power(1) ) {
            ERROR("failed to power on GPS");
            return EXIT_FAILURE;
        }
    }
    
    DEBUG("");
    
    // start read thread
    if ( _read_thread == NULL ) {
        _read_thread = read_thread_init( _gps_state );
        if ( _read_thread == NULL )
            goto failure;
    }
    if ( !_read_thread->active ) {
        if (FAILED( read_thread_start( _read_thread ) ))
            goto failure;
    }
    
    // start write thread
    if ( _write_thread == NULL ) {
        _write_thread = write_thread_init( _gps_state );
        if ( _write_thread == NULL ) 
            goto failure;
    }
    if ( !_write_thread->active ) {
        if (FAILED( write_thread_start( _write_thread ) ))
            goto failure;
    }
    
    // start work thread
    if ( _work_thread == NULL ) {
        WorkState ws;
        ws.active = 0;
        ws.gpsState = _gps_state;
        ws.readQueue = _read_thread->msgQueue;
        ws.writeQueue = _write_thread->msgQueue;

        _work_thread = work_thread_init( &ws );
        if ( _work_thread == NULL ) 
            goto failure;
    }
    if ( !_work_thread->workState.active ) {
        if (FAILED( work_thread_start( _work_thread ) ))
            goto failure;
    }
    
    return EXIT_SUCCESS;

failure:
    gps_cleanup();
    return EXIT_FAILURE;
}

static int gps_stop()
{
    if ( _gps_state == NULL ) {
        ERROR("uninitialized");
        return EXIT_FAILURE;
    }
    
    if ( _work_thread == NULL ) {
        ERROR("not active");
        return EXIT_FAILURE;
    }
    
    DEBUG("");
    work_thread_stop( _work_thread );
    work_thread_free( _work_thread );
    _work_thread = NULL;

    return EXIT_SUCCESS;
}

static int gps_inject_time(GpsUtcTime time, int64_t timeReference, int uncertainty)
{
    if (_gps_state == NULL) {
        ERROR("uninitialized");
        return EXIT_FAILURE;
    }

    ERROR("unsupported");
    return EXIT_FAILURE;
}

static int gps_inject_location(double latitude, double longitude, float accuracy)
{
    GpsLocation loc;

    if (_gps_state == NULL) {
        ERROR("uninitialized");
        return EXIT_FAILURE;
    }

    DEBUG("");
    
    loc.size        = sizeof(GpsLocation);
    loc.flags       = (GPS_LOCATION_HAS_LAT_LONG | GPS_LOCATION_HAS_ACCURACY);
    
    loc.latitude    = latitude;
    loc.longitude   = longitude;
    
    loc.accuracy    = accuracy;
    loc.timestamp   = NOW();
    
    gps_state_set_location( _gps_state, &loc );
    
    return EXIT_SUCCESS;
}

static void gps_delete_aiding_data(GpsAidingData flags)
{
    if (_gps_state == NULL) {
        ERROR("uninitialized");
        return;
    }

    DEBUG("flags=%d", flags);
}

static int gps_set_position_mode(GpsPositionMode mode, GpsPositionRecurrence recurrence,
            UINT32 min_interval, UINT32 preferred_accuracy, UINT32 preferred_time)
{
    if (_gps_state == NULL) {
        ERROR("uninitialized");
        return EXIT_FAILURE;
    }

    DEBUG("mode=%d recurrence=%d min_interval=%d preferred_accuracy=%d preferred_time=%d", 
        mode, recurrence, min_interval, preferred_accuracy, preferred_time);

    return EXIT_SUCCESS;
}

static const void* gps_get_extension(const char* name)
{
    if (_gps_state == NULL) {
        ERROR("uninitialized");
        return NULL;
    }

    DEBUG("name=%s", name);
    return NULL;
}

static const GpsInterface sirfGpsInterface = {
    .size = sizeof(GpsInterface),
    .init = gps_init,
    .start = gps_start,
    .stop = gps_stop,
    .cleanup = gps_cleanup,
    .inject_time = gps_inject_time,
    .inject_location = gps_inject_location,
    .delete_aiding_data = gps_delete_aiding_data,
    .set_position_mode = gps_set_position_mode,
    .get_extension = gps_get_extension,
};

const GpsInterface* gps_get_hardware_interface(struct gps_device_t* dev)
{
    return &sirfGpsInterface;
}

static int open_gps(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
    struct gps_device_t *dev = calloc(1, sizeof(struct gps_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->get_gps_interface = gps_get_hardware_interface;
    
    *device = (struct hw_device_t*)dev;
    return EXIT_SUCCESS;
}

static struct hw_module_methods_t gps_module_methods = {
    .open = open_gps
};

const struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = GPS_HARDWARE_MODULE_ID,
    .name = "SIRF GSD4e GPS Module",
    .author = "WIMM Labs, Inc.",
    .methods = &gps_module_methods,
};
