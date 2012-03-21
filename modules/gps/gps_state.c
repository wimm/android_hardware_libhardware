/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 */


#include "gps_state.h"

// Send dispatch messages on separate thread from work thread.
// Android may call "gps_stop" during a location dispatch and we would deadlock
// if "work_thread_stop" was called from the work thread.
typedef struct {
    volatile UINT8          active;
    UINT8                   flags;
    
    pthread_mutex_t         mutex;
    pthread_cond_t          signal;
    Thread*                 thread;
} GpsDispatchThread;

// Keep data shared between threads protected by obfuscating it in an internal struct.
// This way, callers are forced to use the getter/setter methods instead of accessing ivars directly.
typedef struct {
    GpsDispatchThread*      dispatchThread;
    pthread_mutex_t         mutex;
    
    UINT32                  flags;
    GpsProtocol             protocol;
    GpsGeoRates             geoRates;
    GpsStatusValue          status;
    GpsLocation             location;
    UINT8                   satUsed[GPS_MAX_SVS];
    GpsSvStatus             svStatus;
} GpsStateInternal;


/*
 * THREAD
 */
 
void _gps_state_dispatch_status( GpsState* s )
{
    GpsStatus st;
    GpsStatusValue status = gps_state_get_status( s );
    
    if (s->callbacks.status_cb) {
        INFO("status=%d", status);
        st.size = sizeof(GpsStatus);
        st.status = status;
        s->callbacks.status_cb(&st);
    }
}
 
void _gps_state_dispatch_location( GpsState* s )
{
    GpsLocation loc;
    gps_state_get_location( s, &loc );
    
    if ( loc.timestamp == 0 ) {
        WARN("missing timestamp");
    } else if (loc.flags == 0) {
        WARN("location has no attributes");
    } else if (s->callbacks.location_cb ) {
        INFO("flags=%d", loc.flags);
        s->callbacks.location_cb( &loc );
    }
}

void _gps_state_dispatch_sv_status( GpsState* s )
{
    GpsSvStatus svStatus;
    gps_state_get_sv_status( s, &svStatus );
    
    if (s->callbacks.sv_status_cb) {
        INFO("num_svs=%d", svStatus.num_svs);
        s->callbacks.sv_status_cb( &svStatus );
    }
}

static void gps_state_dispatch_thread( void* arg )
{
    GpsState* s = (GpsState*)arg;
    GpsStateInternal* intl = (GpsStateInternal*)s->internal;
    GpsDispatchThread* t = intl->dispatchThread;
    
    INFO("starting");
    thread_enter( t->thread );
    
    while ( t->active ) {
        UINT8 i, flags;
    
        // wait for signal
        int err = pthread_cond_wait(&t->signal, &t->mutex);
        if ( err ) {
            ERROR("failed at pthread_cond_wait: (%d) %s", err, strerror(err));
            goto end;
        }
        
        pthread_mutex_lock(&intl->mutex);
        flags = t->flags;
        t->flags = 0;
        pthread_mutex_unlock(&intl->mutex);
        
        // dispatch messages
        for (i=0; i<GPS_DISPATCH_MAX; i++) {
            if ( ( flags & (1<<i) ) != 0 ) {
                switch (i) {
                    case GPS_DISPATCH_STATUS:
                        _gps_state_dispatch_status( s );
                        break;
                        
                    case GPS_DISPATCH_LOCATION:
                        _gps_state_dispatch_location( s );
                        break;
                        
                    case GPS_DISPATCH_SV_STATUS:
                        _gps_state_dispatch_sv_status( s );
                        break;
                        
                    default:
                        WARN("reached unknown flag");
                        break;
                }
            }
        }
    }
    
end:
    INFO("exiting (active=%d)", t->active);
    
    // cleanup
    if ( t->active ) {
        t->active = 0;   
        thread_free( t->thread );
        t->thread = NULL;
    } else {    
        thread_exit( t->thread );
    }
    
    INFO("exited");
}


GpsDispatchThread* gps_state_init_dispatch_thread(GpsState* s)
{
    GpsDispatchThread* t;
    
    t = (GpsDispatchThread*) calloc( 1, sizeof( GpsDispatchThread ) );
    
    pthread_mutex_init(&t->mutex, NULL);
    pthread_cond_init(&t->signal, NULL);
    
    t->active = 1;
    t->thread = gps_state_create_thread( 
        s, "gps_dispatch_thread", gps_state_dispatch_thread, s
    );
    
    return t;
}

void gps_state_free_dispatch_thread(GpsDispatchThread* t)
{
    if ( !t ) return;
    
    t->active = 0;
    t->flags = 0;
    pthread_cond_signal(&t->signal);
    thread_join( t->thread );    
    thread_free( t->thread );
    t->thread = NULL;
    
    pthread_cond_destroy( &t->signal );
    pthread_mutex_destroy( &t->mutex );
    
    free( t );
}


/*
 * PROPERTIES
 */

GpsStateInternal* gps_state_lock( GpsState* s )
{
    GpsStateInternal *intl = (GpsStateInternal*)s->internal;
    pthread_mutex_lock(&intl->mutex);
    return intl;
}

void gps_state_unlock( GpsState* s )
{
    GpsStateInternal *intl = (GpsStateInternal*)s->internal;
    pthread_mutex_unlock(&intl->mutex);
}

int gps_state_get_flag( GpsState* s, GpsStateFlag flag )
{
    int res = 0;
    GpsStateInternal *intl = gps_state_lock( s );
    res = ( intl->flags & (1<<flag) ) != 0;
    gps_state_unlock( s );
    return res;
}

void gps_state_set_flag( GpsState* s, GpsStateFlag flag, int set )
{
    GpsStateInternal *intl = gps_state_lock( s );
    if ( set ) intl->flags |= (1<<flag);
    else intl->flags &= ~(1<<flag);
    gps_state_unlock( s );
}

void gps_state_get_geo_rates( GpsState* s, GpsGeoRates* rates )
{
    if ( !rates ) return;
    
    GpsStateInternal *intl = gps_state_lock( s );
    *rates = intl->geoRates;
    gps_state_unlock( s );
}

GpsProtocol gps_state_get_protocol( GpsState* s )
{
    GpsProtocol p;
    GpsStateInternal *intl = gps_state_lock( s );
    p = intl->protocol;
    gps_state_unlock( s );
    return p;
}

void gps_state_set_protocol( GpsState* s, GpsProtocol p )
{
    GpsStateInternal *intl = gps_state_lock( s );
    intl->protocol = p;
    gps_state_unlock( s );
}

GpsStatusValue gps_state_get_status( GpsState* s )
{
    GpsStatusValue status;
    GpsStateInternal *intl = gps_state_lock( s );
    status = intl->status;
    gps_state_unlock( s );
    return status;
}

void gps_state_set_status( GpsState* s, GpsStatusValue status )
{
    DEBUG("status=%d", status);
    GpsStateInternal *intl = gps_state_lock( s );
    intl->status = status;
    gps_state_unlock( s );
}

void gps_state_get_location( GpsState* s, GpsLocation* loc )
{
    if ( !loc ) return;
    
    GpsStateInternal *intl = gps_state_lock( s );
    *loc = intl->location;
    gps_state_unlock( s );
}
 
void gps_state_set_location( GpsState* s, GpsLocation* loc )
{
    GpsStateInternal *intl = gps_state_lock( s );
    intl->location = *loc;
    gps_state_unlock( s );
}

int gps_state_get_sat_used( GpsState* s, UINT8 prn )
{
    int i, p, res = 0;
    GpsStateInternal *intl = gps_state_lock( s );
    
    for (i=0; i<GPS_MAX_SVS; i++) {
        p = intl->satUsed[i];
        
        // end of list
        if ( p == 0 ) break;
        
        // matching
        if ( prn == p ) {
            res = 1;
            break;
        }
    }
    
    gps_state_unlock( s );
    return res;
}

void gps_state_set_sat_used( GpsState* s, UINT8* prns, int len )
{
    if ( len > GPS_MAX_SVS ) return;
    
    GpsStateInternal *intl = gps_state_lock( s );
    memcpy( intl->satUsed, prns, len );
    gps_state_unlock( s );
}

void gps_state_clear_sat_used( GpsState* s )
{
    GpsStateInternal *intl = gps_state_lock( s );
    memset( intl->satUsed, 0, GPS_MAX_SVS );
    gps_state_unlock( s );
}

void gps_state_get_sv_status( GpsState* s, GpsSvStatus* status )
{
    if ( !status ) return;
    
    GpsStateInternal *intl = gps_state_lock( s );
    *status = intl->svStatus;
    gps_state_unlock( s );
}

int gps_state_add_sv_status( GpsState* s, GpsSvInfo* info )
{
    int svIdx = -1;
    GpsStateInternal *intl = gps_state_lock( s );
    GpsSvStatus* svStatus = &intl->svStatus;
    
    svIdx = svStatus->num_svs;
    if ( svIdx >= GPS_MAX_SVS ) {
        ERROR("sat over-run");
        goto end;
    }
    
    // copy info
    svStatus->sv_list[svIdx] = *info;
    
    // increment count
    svStatus->num_svs++;
    svIdx++;
        
    // update mask
    if ( gps_state_get_sat_used( s, info->prn ) ) {
        if ( info->prn > 32 ) ERROR("prn > 32");
        else svStatus->used_in_fix_mask |= ( 1<<(32 - info->prn) );
    }
    
    // TODO? ephemeris_mask
    // TODO? almanac_mask
    
end:
    gps_state_unlock( s );
    return svIdx;
}

void gps_state_clear_sv_status( GpsState* s )
{
    GpsStateInternal *intl = gps_state_lock( s );
    memset(&intl->svStatus, 0, sizeof(GpsSvStatus));
    intl->svStatus.size = sizeof(GpsSvStatus);
    gps_state_unlock( s );
}


/*
 * CALLBACKS
 */

Thread* gps_state_create_thread( GpsState* s, const char* name, void (*start)(void *), void* arg )
{
    Thread *t = thread_alloc();
    t->thread = s->callbacks.create_thread_cb( name, start, arg );

    if ( !t->thread ) {
        ERROR("could not create thread %s: (%d) %s", name, errno, strerror(errno));
        
        thread_free( t );
        return NULL;
    }
    
    return t;
}

void gps_state_dispatch( GpsState* s, GpsDispatchCode code )
{
    GpsStateInternal *intl = gps_state_lock( s );
    intl->dispatchThread->flags |= (1<<code);
    pthread_cond_signal(&intl->dispatchThread->signal);
    gps_state_unlock( s );
}

void gps_state_acquire_wakelock( GpsState* s )
{
    DEBUG("");
    s->callbacks.acquire_wakelock_cb();
}

void gps_state_release_wakelock( GpsState* s )
{
    DEBUG("");
    s->callbacks.release_wakelock_cb();
}


/*
 * CONSTRUCTION
 */

GpsState* gps_state_init( GpsCallbacks* callbacks )
{
    GpsState* s;
    GpsStateInternal* intl;
    pthread_mutexattr_t attr;
    
    DEBUG("");
    
    s = (GpsState*) calloc( 1, sizeof( GpsState ) );
    s->callbacks = *callbacks;
    
    if (FAILED( gps_comm_open(&s->comm, GPS_DEV_PATH) )) {
        free( s );
        return NULL;
    }
    
    intl = (GpsStateInternal*) calloc( 1, sizeof( GpsStateInternal ) );
    s->internal = intl;
    
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&intl->mutex, &attr);
    
    intl->geoRates = (GpsGeoRates) { 1, 0, 1, 5, 1, 0, 0, 0, 0 };
    intl->dispatchThread = gps_state_init_dispatch_thread( s );
    
    return s;
}

void gps_state_free( GpsState* s )
{
    GpsStateInternal* intl;
    
    if ( !s ) return;
    
    intl = (GpsStateInternal*)s->internal;
    gps_state_free_dispatch_thread( intl->dispatchThread );
    pthread_mutex_destroy( &intl->mutex );
    free( intl );
    
    free( s );
}
