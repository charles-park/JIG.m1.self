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
#define STR_PATH_LENGTH 128

struct device_audio {
    // find flag
    char is_file;
    // Control file name
    const char *fname;
    // play time (sec)
    int play_time;
    char path[STR_PATH_LENGTH];
};

//------------------------------------------------------------------------------
//
// Configuration
//
//------------------------------------------------------------------------------
/* define audio devices */
//------------------------------------------------------------------------------
#define PLAY_TIME_SEC   1

struct device_audio DeviceAUDIO [eAUDIO_END] = {
    // AUDIO LEFT
    { 0, "1khz_left.wav" , PLAY_TIME_SEC, { 0, } },
    // AUDIO RIGHT
    { 0, "1khz_right.wav", PLAY_TIME_SEC, { 0, } },
};

//------------------------------------------------------------------------------
// return 1 : find success, 0 : not found
//------------------------------------------------------------------------------
static int find_file_path (const char *fname, char *file_path)
{
    FILE *fp;
    char cmd_line[STR_PATH_LENGTH];

    memset (cmd_line, 0, sizeof(cmd_line));
    sprintf(cmd_line, "%s\n", "pwd");

    if (NULL != (fp = popen(cmd_line, "r"))) {
        memset (cmd_line, 0, sizeof(cmd_line));
        fgets  (cmd_line, STR_PATH_LENGTH, fp);
        pclose (fp);

        strncpy (file_path, cmd_line, strlen(cmd_line)-1);

        memset (cmd_line, 0, sizeof(cmd_line));
        sprintf(cmd_line, "find -name %s\n", fname);
        if (NULL != (fp = popen(cmd_line, "r"))) {
            memset (cmd_line, 0, sizeof(cmd_line));
            fgets  (cmd_line, STR_PATH_LENGTH, fp);
            pclose (fp);
            if (strlen(cmd_line)) {
                strncpy (&file_path[strlen(file_path)], &cmd_line[1], strlen(cmd_line)-1);
                file_path[strlen(file_path)-1] = ' ';
                return 1;
            }
            return 0;
        }
    }
    pclose(fp);
    return 0;
}

//------------------------------------------------------------------------------
// thread control variable
//------------------------------------------------------------------------------
volatile int AudioBusy = 0;

void *audio_thread_func (void *arg)
{
    FILE *fp;
    char cmd [STR_PATH_LENGTH *2];

    struct device_audio *audio = (struct device_audio *)arg;

    while (AudioBusy)   sleep (1);

    AudioBusy = 1;
    memset  (cmd, 0, sizeof(cmd));
    sprintf (cmd, "aplay -Dhw:0,0 %s -d %d && sync",
        audio->path, audio->play_time);
    if ((fp = popen (cmd, "r")) != NULL)
        pclose(fp);

    AudioBusy = 0;
    return arg;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int audio_check (int id)
{
    pthread_t audio_thread;

    memset (DeviceAUDIO[id].path, 0, STR_PATH_LENGTH);
    DeviceAUDIO[id].is_file =
        find_file_path (DeviceAUDIO[id].fname, DeviceAUDIO[id].path);

    if ((id >= eAUDIO_END) || (DeviceAUDIO[id].is_file != 1)) {
        return 0;
    }
    if (!AudioBusy)
        pthread_create (&audio_thread, NULL, audio_thread_func, &DeviceAUDIO[id]);

    return AudioBusy ? 2 : 1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
