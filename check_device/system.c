//------------------------------------------------------------------------------
/**
 * @file system.c
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
#include <sys/sysinfo.h>

//------------------------------------------------------------------------------
#include "system.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define STR_PATH_LENGTH 128

struct device_system {
    // device check value
    int res_x;
    int res_y;
    char fb_path[STR_PATH_LENGTH +1];

    // read data
    int mem;
    int r_res_x;
    int r_res_y;

};

//------------------------------------------------------------------------------
//
// Configuration
//
//------------------------------------------------------------------------------
/* define system devices */
//------------------------------------------------------------------------------
// Fixed resolution
#define DEFAULT_RES_X   1920
#define DEFAULT_RES_Y   1080

static struct device_system DeviceSYSTEM = {
    DEFAULT_RES_X, DEFAULT_RES_Y, "/sys/class/graphics/fb0/virtual_size", 0, 0, 0
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int get_memory_size (void)
{
    struct sysinfo sinfo;
    int mem_size = 0;

    if (!sysinfo (&sinfo)) {
        if (sinfo.totalram) {

            mem_size = sinfo.totalram / 1024 / 1024;

            switch (mem_size) {
                case    8193 ... 16384: mem_size = 16;  break;
                case    4097 ... 8192:  mem_size = 8;   break;
                case    2049 ... 4096:  mem_size = 4;   break;
                case    1025 ... 2048:  mem_size = 2;   break;
                default :
                    break;
            }
        }
    }
    return mem_size;
}

//------------------------------------------------------------------------------
static int get_fb_size (const char *path, int id)
{
    FILE *fp;
    int x, y;

    if (access (path, R_OK) == 0) {
        if ((fp = fopen(path, "r")) != NULL) {
            char rdata[16], *ptr;

            memset (rdata, 0x00, sizeof(rdata));

            if (fgets (rdata, sizeof(rdata), fp) != NULL) {
                if ((ptr = strtok (rdata, ",")) != NULL)
                    x = atoi(ptr);

                if ((ptr = strtok (NULL, ",")) != NULL)
                    y = atoi(ptr);
            }
            fclose(fp);
        }
        switch (id) {
            case eSYSTEM_FB_X:  return x;
            case eSYSTEM_FB_Y:  return y;
            default :           return 0;
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
int system_check (int id)
{
    switch (id) {
        case eSYSTEM_MEM:
            return get_memory_size();
        case eSYSTEM_FB_X:  case eSYSTEM_FB_Y:

            if (access (DeviceSYSTEM.fb_path, R_OK) == 0)
                return  get_fb_size (DeviceSYSTEM.fb_path, id);
            return 0;
        default :
            break;
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
