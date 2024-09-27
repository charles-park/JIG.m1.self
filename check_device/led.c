//------------------------------------------------------------------------------
/**
 * @file led.c
 * @author charles-park (charles.park@hardkernel.com)
 * @brief Device Test library for ODROID-JIG.
 * @version 0.2
 * @date 2023-10-12
 *
 * @package apt install iperf3, nmap, ethtool, usbutils, alsa-utils
 *
 * @copyright Copyright (c) 2022
 *
 */
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/sysinfo.h>

//------------------------------------------------------------------------------
#include "led.h"

//------------------------------------------------------------------------------
struct device_led {
    // Control path
    const char *path;
    // set str
    const char *set;
    // clear str
    const char *clr;
};

//------------------------------------------------------------------------------
//
// Configuration
//
//------------------------------------------------------------------------------
/* define led devices */
//------------------------------------------------------------------------------
struct device_led DeviceLED [eLED_END] = {
    // eLED_POWER
    { "/sys/class/leds/power/brightness", "0", "255" },
    // eLED_ALIVE
    { "/sys/class/leds/work/brightness" , "255", "0" },
};

const char *ALIVE_TRIGGER = "/sys/class/leds/blue/trigger";

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int led_read (const char *path)
{
    char rdata[16];
    FILE *fp;

    memset (rdata, 0, sizeof (rdata));

    // led value get
    if ((fp = fopen(path, "r")) != NULL) {
        fread (rdata, 1, sizeof(rdata), fp);
        fclose(fp);
    }

    return atoi(rdata);
}

//------------------------------------------------------------------------------
static int led_write (const char *path, const char *wdata)
{
    FILE *fp;

    // led value set
    if ((fp = fopen(path, "w")) != NULL) {
        fwrite (wdata, 1, strlen(wdata), fp);
        fclose(fp);
    }

    return atoi(wdata);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int led_set_status (int id, int onoff)
{
    if ((id >= eLED_END) || (access (DeviceLED[id].path, R_OK) != 0)) {
        return 0;
    }

    led_write (DeviceLED[id].path, onoff ? DeviceLED[id].set : DeviceLED[id].clr);

    return 1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
