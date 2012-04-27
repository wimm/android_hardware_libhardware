/*
 * Copyright (C) 2012 WIMM Labs, Inc.
 *
 * by Zach Rosen, April 26, 2012
 */


ARCHITECTURE:

The GPS driver is loaded as a shared library into Android's "system_server" process.  The interface for the shared library is defined in "interface.c".  There are four methods that Android uses to control the GPS state:
    * gps_init:     Called only once, either at boot or when GPS is enabled.
    * gps_cleanup:  Called each time the GPS listener count goes to 0, or when GPS is disabled.
    * gps_start:    Called when GPS listeners are registered and a fix is requested.
    * gps_stop:     Called when GPS listeners are registered, but no fix is needed.

The GPS driver uses 5 threads:
    * gps_read_thread:        Listens for incoming serial data, parses the data into messages and adds the messages to the "work" queue.
    * gps_write_thread:       Waits for messages to arrive on the "write" queue, translates the messages into data and sends the data over the serial port.
    * gps_work_thread:        Handles device initialization, sleep/wake modes and responding to messages on the "work" queue.
    * gps_dispatch_thread:    Performs callbacks to Android.  Dispatching from a separate thread protects agains re-entrant behavior as Android may decide to call gps_stop during a callback.
    * gps_sgee_thread:        Sends SGEE file to the GPS chip.  The GPS chip will send host file updates while the SGEE file is being uploaded.  Handling SGEE in a separate allows the work thread to handle file updates in parallel.

When "gps_init" is called, we store the GpsCallbacks object and start the dispatch thread.

When "gps_start" is called, the GPS chip is powered on and the read, write and work threads are started.  The work thread grabs a wake lock and loads the patch file onto the chip.  Once this is complete, the work thread starts processing incoming messages.  If an SGEE update is needed, the work thread launches the SGEE thread.

When "gps_stop" is called, the work thread places the chip into hibernate mode and exits.  The read thread is left running so that we can detect if the chip was unable to enter hibernate mode.

When "gps_cleanup" is called, we stop the read and write threads and power off the chip.


KNOWN ISSUES:

ISSUE: Location/Time injection is not be working correctly.  
NOTES: After SGEE upload, but without any injection, the GSV messages show satellites 1-12 with all 0 values for SNR and position.  After sending the LLA Nav Init command (WARM_RESET_WITH_INIT) with location and time, the GSV messages change to show the satellites that are currently visible.  However, after several seconds of exposure to open sky, the positions of the satellites given by the GSV messages shift dramatically.  This shift may indicate that the currently visible satellites are not being calculated correctly.  Additionally, if the WARM_RESET_WITH_INIT is issued with SGEE already loaded (after exiting hibernation, for example) the chip sends a request for a new SGEE file.  This should not happen; the chip should be ok with the SGEE already loaded.

ISSUE: GPS is not disabled in "low power mode".
NOTES: com.wimm.system.PowerService should be updated to enabled/disable GPS.

ISSUE: Driver may be left in bad state if "gps_stop" or "gps_cleanup" being called during the patching process.
NOTES: Investigate.

ISSUE: GPS patch file must be manually placed at "/data/gps/patch.pd2".  
NOTES: The patch file should be built into the system image.

ISSUE: SGEE file must be manually placed at "/data/gps/sgee.bin".  
NOTES: The SyncService should download the SGEE file from the server.

ISSUE: SGEE is only updated if no SGEE exists in host storage, or if the chip requests a new SGEE.
NOTES: We should use SGEE age polling and notifications (or timestamps) from the SyncService to see if a new SGEE file should be sent to the chip.

ISSUE: Some threads are running at unnecessary times.
NOTES: These threads are blocked, so they're not consuming CPU, but we could recover their memory overhead.
The "dispatch" thread is started at "gps_init" and never exits.  This thread could be started at "gps_start" and exited at "gps_cleanup".  The "write" thread is not exited until "gps_cleanup".  This thread could be exited at "gps_stop".

