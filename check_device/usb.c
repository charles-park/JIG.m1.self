//------------------------------------------------------------------------------
/**
 * @file usb.c
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

//------------------------------------------------------------------------------
#include "usb.h"

//------------------------------------------------------------------------------
#define STR_PATH_LENGTH 128

struct device_usb {
    // Control path
    char path[STR_PATH_LENGTH +1];
    // compare value (read min, write min : MB/s)
    int r_min, w_min;
    // Link speed
    int speed;

    // read value (Init)
    int value;
};

// default link speed, read/write (MB/s)
#define DEFAULT_USB30_L 5000
#define DEFAULT_USB30_R 100
#define DEFAULT_USB30_W 35

#define DEFAULT_USB20_L 480
#define DEFAULT_USB20_R 25
#define DEFAULT_USB20_W 20

//------------------------------------------------------------------------------
//
// Configuration
//
//------------------------------------------------------------------------------
/* define usb devices (USB3.0 eMMC reader with eMMC) */
//------------------------------------------------------------------------------
struct device_usb DeviceUSB [eUSB_END] = {
    // path, r_min(MB/s), w_min(MB/s), link, read
    // eUSB_30, USB 3.0
    { "/sys/bus/usb/devices/8-1", DEFAULT_USB30_R, DEFAULT_USB30_W, DEFAULT_USB30_L, 0 },
    { "/sys/bus/usb/devices/6-1", DEFAULT_USB30_R, DEFAULT_USB30_W, DEFAULT_USB30_L, 0 },
    // eUSB_20, USB 2.0
    { "/sys/bus/usb/devices/1-1", DEFAULT_USB20_R, DEFAULT_USB20_W, DEFAULT_USB20_L, 0 },
    { "/sys/bus/usb/devices/2-1", DEFAULT_USB20_R, DEFAULT_USB20_W, DEFAULT_USB20_L, 0 },
    // eUSB_C, USB 3.0
};

// USB Read / Write (16 Mbytes, 1 block count)
//#define USB_R_CHECK "dd of=/dev/null bs=16M count=1 iflag=nocache,dsync oflag=nocache,dsync if=/dev/"
#define USB_R_CHECK "dd of=/dev/null bs=16M count=1 iflag=direct,dsync oflag=nocache,dsync if=/dev/"
#define USB_W_CHECK "dd if=/dev/zero bs=16M count=1 iflag=nocache,dsync oflag=nocache,dsync of=/dev/"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int usb_speed (const char *path)
{
    FILE *fp;
    char cmd[STR_PATH_LENGTH], rdata[STR_PATH_LENGTH];

    if (access (path, R_OK) == 0) {
        memset  (cmd, 0x00, sizeof(cmd));
        sprintf (cmd, "%s/speed", path);
        if ((fp = fopen (cmd, "r")) != NULL) {
            memset (rdata, 0, sizeof(rdata));
            fgets  (rdata, sizeof(rdata), fp);
            fclose (fp);
        }
        return atoi (rdata);
    }
    return 0;
}

//------------------------------------------------------------------------------
static int _usb_rw (const char *path, const char *check_cmd)
{
    FILE *fp;
    char cmd[STR_PATH_LENGTH], rdata[STR_PATH_LENGTH], *ptr;

    memset  (cmd, 0x00, sizeof(cmd));
    sprintf (cmd, "find %s/ -name sd* 2>&1", path);

    if ((fp = popen (cmd, "r")) != NULL) {
        memset (rdata, 0x00, sizeof(cmd));
        // 1 line read
        fgets (rdata, sizeof(rdata), fp);
        fclose (fp);
        // find string "sd"
        if ((ptr = strstr (rdata, "sd")) != NULL) {
            memset  (cmd, 0, sizeof (cmd));
            // 0x00 -> 0x20 (space)
            ptr[strlen(ptr)-1] = ' ';
            sprintf (cmd, "%s%s 2>&1", check_cmd, ptr);

            if ((fp = popen(cmd, "r")) != NULL) {
                while (1) {
                    memset (rdata, 0, sizeof (rdata));
                    if ((fgets (rdata, sizeof (rdata), fp)) == NULL)
                        break;
                    if ((ptr = strstr (rdata, " MB/s")) != NULL) {
                        while (*ptr != ',') ptr--;
                        return atoi (ptr+1);
                    }
                }
                pclose (fp);
            }
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
int usb_check (int id)
{
    if ((id >= eUSB_END) || ((access (DeviceUSB[id].path, R_OK)) != 0))
        return 0;

    if (usb_speed (DeviceUSB[id].path) != DeviceUSB[id].speed)
        return 0;

    return 1;
}

//------------------------------------------------------------------------------
int usb_rw (int id)
{
    int value = 0;

    if (usb_check(id)) {
        sleep(1);
        switch (id) {
            case eUSB30_UP_W:   case eUSB30_DN_W:
            case eUSB20_UP_W:   case eUSB20_DN_W:
                value = _usb_rw (DeviceUSB[id].path, USB_W_CHECK);
                return (value > DeviceUSB[id].w_min) ? value : 0;
            default :
                value = _usb_rw (DeviceUSB[id].path, USB_R_CHECK);
                return (value > DeviceUSB[id].r_min) ? value : 0;
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
