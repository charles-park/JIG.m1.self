//------------------------------------------------------------------------------
/**
 * @file hdmi.c
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
#include <sys/sysinfo.h>

//------------------------------------------------------------------------------
#include "hdmi.h"

//------------------------------------------------------------------------------
struct device_led {
    // Control path
    const char *path;

    const char *pass_str;
    // 1 = pass, 0 = fail
    int value;
};

//------------------------------------------------------------------------------
//
// Configuration
//
//------------------------------------------------------------------------------
/* define hdmi devices */
//------------------------------------------------------------------------------
#define HDMI_READ_BYTES 32

struct device_led DeviceHDMI [eHDMI_END] = {
    // EDID
    {
        "/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/edid",
        /* 16 char string (8bytes raw data)*/
        "00FFFFFFFFFFFF00",
        0
    },
    // HPD
    {
        "/sys/devices/platform/display-subsystem/drm/card0/card0-HDMI-A-1/status",
        "connected",
        0
    },
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int hdmi_read (const char *path, char *rdata)
{
    FILE *fp;

    // led value get
    if ((fp = fopen(path, "r")) != NULL) {
        fread (rdata, 1, HDMI_READ_BYTES, fp);
        fclose(fp);
        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
static int data_check (int id, const char *rdata)
{
    char buf[HDMI_READ_BYTES];
    memset (buf, 0, sizeof(buf));

    switch (id) {
        case eHDMI_EDID:
            sprintf (buf, "%02X%02X%02X%02X%02X%02X%02X%02X",
                rdata[0], rdata[1], rdata[2], rdata[3],
                rdata[4], rdata[5], rdata[6], rdata[7]);
            break;
        // hpd
        case eHDMI_HPD:
            strncpy (buf, rdata, strlen (DeviceHDMI[id].pass_str));
            break;
        default:
            break;
    }

    if (!strncmp (buf,
        DeviceHDMI[id].pass_str, strlen (DeviceHDMI[id].pass_str) -1))
        return 1;

    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int hdmi_check (int id)
{
    int value = 0;
    char rdata[HDMI_READ_BYTES];

    if ((id >= eHDMI_END) || (access (DeviceHDMI[id].path, R_OK) != 0)) {
        return 0;
    }

    memset (rdata, 0, sizeof(rdata));

    if (hdmi_read (DeviceHDMI[id].path, rdata))
        value = data_check (id, rdata);

    return value;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
