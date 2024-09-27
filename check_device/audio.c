//------------------------------------------------------------------------------
/**
 * @file audio.c
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
#include "audio.h"

//------------------------------------------------------------------------------
int audio_check (int id)
{
    FILE *fp;
    char cmd [128];

    // check speaker-test file
    if (access ("/usr/bin/speaker-test", F_OK)) return 0;

    if ((id != eAUDIO_LEFT) && (id != eAUDIO_RIGHT))
        return 0;

    memset  (cmd, 0, sizeof(cmd));
    sprintf (cmd, "/usr/bin/speaker-test -Dhw:1,0 -t wav -c2 -s%d && sync", id);

    if ((fp = popen (cmd, "w")) != NULL)
        pclose(fp);

    return 1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
