//------------------------------------------------------------------------------
/**
 * @file storage.c
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
#include "storage.h"

//------------------------------------------------------------------------------
#define STR_PATH_LENGTH 128

struct device_storage {
    // Control path
    char path [STR_PATH_LENGTH +1];
    // compare value (read min, write min : MB/s)
    int r_min, w_min;

    // read value
    int value;
};

/* Device default r/w speed (MB/s) */
#define DEFAULT_EMMC_R  140
#define DEFAULT_EMMC_W  70

#define DEFAULT_uSD_R   50
#define DEFAULT_uSD_W   20

#define DEFAULT_SATA_R  350
#define DEFAULT_SATA_W  150

#define DEFAULT_NVME_R  700
#define DEFAULT_NVME_W  150

//------------------------------------------------------------------------------
//
// Configuration
//
//------------------------------------------------------------------------------
/* define storage devices */
//------------------------------------------------------------------------------
// Boot device define (uSD)
#define BOOT_DEVICE     eSTORAGE_uSD_W
#define TEMP_FILE       "/tmp/wdat"

struct device_storage DeviceSTORAGE [eSTORAGE_END] = {
    // path, r_min(MB/s), w_min(MB/s), read
    // eSTORAGE_EMMC
    { "/dev/mmcblk0", DEFAULT_EMMC_R, DEFAULT_EMMC_W,   0 },
    // eSTORAGE_uSD (boot device : /root)
    { "/dev/mmcblk1",  DEFAULT_uSD_R,  DEFAULT_uSD_R,   0 },
    // eSTORAGE_SATA
    { "/dev/sda"    , DEFAULT_SATA_R, DEFAULT_SATA_R,   0 },
    // eSTORAGE_NVME
    { "/dev/nvme0n1", DEFAULT_NVME_R, DEFAULT_NVME_R,   0 },
};

// Storage Read / Write (16 Mbytes, 1 block count)
//#define STORAGE_R_CHECK "dd of=/dev/null bs=16M count=1 iflag=nocache,dsync oflag=nocache,dsync if="
#define STORAGE_R_CHECK "dd of=/dev/null bs=16M count=1 iflag=direct,dsync oflag=nocache,dsync if="
#define STORAGE_W_CHECK "dd if=/dev/zero bs=16M count=1 iflag=nocache,dsync oflag=nocache,dsync of="

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int remove_tmp (const char *path)
{
    FILE *fp;
    char cmd[STR_PATH_LENGTH];

    if (access (path, R_OK) == 0) {
        memset  (cmd, 0x00, sizeof(cmd));
        sprintf (cmd, "rm -f %s 2>&1", path);

        if ((fp = popen (cmd, "r")) != NULL) {
            pclose(fp);
            return 1;
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
static int _storage_rw (const char *path, const char *check_cmd)
{
    FILE *fp;
    char cmd[STR_PATH_LENGTH], rdata[STR_PATH_LENGTH], *ptr;

    memset  (cmd, 0x00, sizeof(cmd));
    sprintf (cmd, "%s%s 2>&1", check_cmd, path);

    if ((fp = popen (cmd, "r")) != NULL) {
        while (fgets (rdata, sizeof(rdata), fp) != NULL) {
            if ((ptr = strstr (rdata, " MB/s")) != NULL) {
                while (*ptr != ',') ptr--;
                return atoi (ptr+1);
            }
        }
        pclose(fp);
    }
    return 0;
}

//------------------------------------------------------------------------------
int storage_check (int id)
{
    if ((id >= eSTORAGE_END) || (access (DeviceSTORAGE[id].path, R_OK) != 0)) {
        return 0;
    }
    return 1;
}

//------------------------------------------------------------------------------
int storage_rw (int id)
{
    int value = 0;

    if (storage_check(id)) {
        sleep(1);
        switch (id) {
            case eSTORAGE_eMMC_W:   case eSTORAGE_uSD_W:
            case eSTORAGE_NVME_W:   case eSTORAGE_SATA_W:
                if (id == BOOT_DEVICE) {
                    value = _storage_rw (TEMP_FILE, STORAGE_W_CHECK);
                    remove_tmp (TEMP_FILE);
                }
                return value;
            default :
                break;

        }
        value = _storage_rw (DeviceSTORAGE[id].path, STORAGE_R_CHECK);
    }

    return (value > DeviceSTORAGE[id].w_min) ? value : 0;
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
