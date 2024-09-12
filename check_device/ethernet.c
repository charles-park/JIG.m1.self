//------------------------------------------------------------------------------
/**
 * @file ethernet.c
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
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <linux/fb.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

//------------------------------------------------------------------------------
#include "ethernet.h"

#define STR_PATH_LENGTH 128
//------------------------------------------------------------------------------
static int ethernet_link_speed (void)
{
    FILE *fp;
    char cmd_line[STR_PATH_LENGTH];

    if (access ("/sys/class/net/eth0/speed", F_OK) != 0)
        return 0;

    memset (cmd_line, 0x00, sizeof(cmd_line));
    if ((fp = fopen ("/sys/class/net/eth0/speed", "r")) != NULL) {
        memset (cmd_line, 0x00, sizeof(cmd_line));
        if (NULL != fgets (cmd_line, sizeof(cmd_line), fp)) {
            fclose (fp);
            return atoi (cmd_line);
        }
        fclose (fp);
    }
    return 0;
}

//------------------------------------------------------------------------------
int ethernet_link_check (void)
{
    return ethernet_link_speed();
}

//------------------------------------------------------------------------------
int ethernet_link_setup (int speed)
{
    FILE *fp;
    char cmd_line[STR_PATH_LENGTH], retry = 10;

    if (ethernet_link_speed() != speed) {
        memset (cmd_line, 0x00, sizeof(cmd_line));
        sprintf(cmd_line,"ethtool -s eth0 speed %d duplex full", speed);
        if ((fp = popen(cmd_line, "w")) != NULL)
            pclose(fp);
    }
    // timeout 10 sec
    while (retry--) {
        if (ethernet_link_speed() == speed)
            return 1;
        sleep (1);
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
