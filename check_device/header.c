//------------------------------------------------------------------------------
/**
 * @file header.c
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
#include "../lib_gpio/lib_gpio.h"
#include "header.h"

//------------------------------------------------------------------------------
//
// Configuration
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
// ODROID-M1 Header GPIOs Define
//
//------------------------------------------------------------------------------
// Not Control
#define NC  0

const int HEADER40[] = {
    // Header J2 GPIOs
     NC,        // Not used (pin 0)
     NC,  NC,   // | 01 : 3.3V     || 02 : 5.0V     |
     NC,  NC,   // | 03 : GPIO3_B6 || 04 : 5.0V     |
     NC,  NC,   // | 05 : GPIO3_B5 || 06 : GND      |
     14, 126,   // | 07 : GPIO0_B6 || 08 : GPIO3_D6 |
     NC, 127,   // | 09 : GND      || 10 : GPIO3_D7 |
     16, 120,   // | 11 : GPIO0_C0 || 12 : GPIO3_D0 |
     17,  NC,   // | 13 : GPIO0_C1 || 14 : GND      |
    106, 118,   // | 15 : GPIO3_B2 || 16 : GPIO3_C6 |
     NC, 119,   // | 17 : 3.3V     || 18 : GPIO3_C7 |
     89,  NC,   // | 19 : GPIO2_D1 || 20 : GND      |
     88, 121,   // | 21 : GPIO2_D0 || 22 : GPIO3_D1 |
     91,  90,   // | 23 : GPIO2_D3 || 24 : GPIO2_D2 |
     NC, 122,   // | 25 : GND      || 26 : GPIO3_D2 |
     12,  11,   // | 27 : GPIO0_B4 || 28 : GPIO0_B3 |
    145,  NC,   // | 29 : GPIO4_C1 || 30 : GND      |
    142, 123,   // | 31 : GPIO4_B6 || 32 : GPIO3_D3 |
     13,  NC,   // | 33 : GPIO0_B5 || 34 : GND      |
    125, 124,   // | 35 : GPIO3_D5 || 36 : GPIO3_D4 |
     NC,  NC,   // | 37 : ADC.AIN4 || 38 : 1.8V     |
     NC,  NC,   // | 39 : GND      || 40 : ADC.AIN5 |
};

//------------------------------------------------------------------------------
#define PATTERN_COUNT   4

const int H40_PATTERN[PATTERN_COUNT][sizeof(HEADER40)] = {
    // Pattern 0 : ALL High
    {
        // Header J2 GPIOs
         NC,        // Not used (pin 0)
         NC,  NC,   // | 01 : 3.3V     || 02 : 5.0V     |
         NC,  NC,   // | 03 : GPIO3_B6 || 04 : 5.0V     |
         NC,  NC,   // | 05 : GPIO3_B5 || 06 : GND      |
          1,   1,   // | 07 : GPIO0_B6 || 08 : GPIO3_D6 |
         NC,   1,   // | 09 : GND      || 10 : GPIO3_D7 |
          1,   1,   // | 11 : GPIO0_C0 || 12 : GPIO3_D0 |
          1,  NC,   // | 13 : GPIO0_C1 || 14 : GND      |
          1,   1,   // | 15 : GPIO3_B2 || 16 : GPIO3_C6 |
         NC,   1,   // | 17 : 3.3V     || 18 : GPIO3_C7 |
          1,  NC,   // | 19 : GPIO2_D1 || 20 : GND      |
          1,   1,   // | 21 : GPIO2_D0 || 22 : GPIO3_D1 |
          1,   1,   // | 23 : GPIO2_D3 || 24 : GPIO2_D2 |
         NC,   1,   // | 25 : GND      || 26 : GPIO3_D2 |
          1,   1,   // | 27 : GPIO0_B4 || 28 : GPIO0_B3 |
          1,  NC,   // | 29 : GPIO4_C1 || 30 : GND      |
          1,   1,   // | 31 : GPIO4_B6 || 32 : GPIO3_D3 |
          1,  NC,   // | 33 : GPIO0_B5 || 34 : GND      |
          1,   1,   // | 35 : GPIO3_D5 || 36 : GPIO3_D4 |
         NC,  NC,   // | 37 : ADC.AIN4 || 38 : 1.8V     |
         NC,  NC,   // | 39 : GND      || 40 : ADC.AIN5 |
    },
    // Pattern 1 : ALL Low
    {
        // Header J2 GPIOs
         NC,        // Not used (pin 0)
         NC,  NC,   // | 01 : 3.3V     || 02 : 5.0V     |
         NC,  NC,   // | 03 : GPIO3_B6 || 04 : 5.0V     |
         NC,  NC,   // | 05 : GPIO3_B5 || 06 : GND      |
          0,   0,   // | 07 : GPIO0_B6 || 08 : GPIO3_D6 |
         NC,   0,   // | 09 : GND      || 10 : GPIO3_D7 |
          0,   0,   // | 11 : GPIO0_C0 || 12 : GPIO3_D0 |
          0,  NC,   // | 13 : GPIO0_C1 || 14 : GND      |
          0,   0,   // | 15 : GPIO3_B2 || 16 : GPIO3_C6 |
         NC,   0,   // | 17 : 3.3V     || 18 : GPIO3_C7 |
          0,  NC,   // | 19 : GPIO2_D1 || 20 : GND      |
          0,   0,   // | 21 : GPIO2_D0 || 22 : GPIO3_D1 |
          0,   0,   // | 23 : GPIO2_D3 || 24 : GPIO2_D2 |
         NC,   0,   // | 25 : GND      || 26 : GPIO3_D2 |
          0,   0,   // | 27 : GPIO0_B4 || 28 : GPIO0_B3 |
          0,  NC,   // | 29 : GPIO4_C1 || 30 : GND      |
          0,   0,   // | 31 : GPIO4_B6 || 32 : GPIO3_D3 |
          0,  NC,   // | 33 : GPIO0_B5 || 34 : GND      |
          0,   0,   // | 35 : GPIO3_D5 || 36 : GPIO3_D4 |
         NC,  NC,   // | 37 : ADC.AIN4 || 38 : 1.8V     |
         NC,  NC,   // | 39 : GND      || 40 : ADC.AIN5 |
    },
    // Pattern 2 : Cross 0
    {
        // Header J2 GPIOs
         NC,        // Not used (pin 0)
         NC,  NC,   // | 01 : 3.3V     || 02 : 5.0V     |
         NC,  NC,   // | 03 : GPIO3_B6 || 04 : 5.0V     |
         NC,  NC,   // | 05 : GPIO3_B5 || 06 : GND      |
          0,   1,   // | 07 : GPIO0_B6 || 08 : GPIO3_D6 |
         NC,   0,   // | 09 : GND      || 10 : GPIO3_D7 |
          1,   0,   // | 11 : GPIO0_C0 || 12 : GPIO3_D0 |
          0,  NC,   // | 13 : GPIO0_C1 || 14 : GND      |
          1,   0,   // | 15 : GPIO3_B2 || 16 : GPIO3_C6 |
         NC,   1,   // | 17 : 3.3V     || 18 : GPIO3_C7 |
          0,  NC,   // | 19 : GPIO2_D1 || 20 : GND      |
          1,   0,   // | 21 : GPIO2_D0 || 22 : GPIO3_D1 |
          0,   1,   // | 23 : GPIO2_D3 || 24 : GPIO2_D2 |
         NC,   0,   // | 25 : GND      || 26 : GPIO3_D2 |
          0,   1,   // | 27 : GPIO0_B4 || 28 : GPIO0_B3 |
          1,  NC,   // | 29 : GPIO4_C1 || 30 : GND      |
          0,   1,   // | 31 : GPIO4_B6 || 32 : GPIO3_D3 |
          1,  NC,   // | 33 : GPIO0_B5 || 34 : GND      |
          0,   1,   // | 35 : GPIO3_D5 || 36 : GPIO3_D4 |
         NC,  NC,   // | 37 : ADC.AIN4 || 38 : 1.8V     |
         NC,  NC,   // | 39 : GND      || 40 : ADC.AIN5 |
    },
    // Pattern 3 : Cross 1
    {
        // Header J2 GPIOs
         NC,        // Not used (pin 0)
         NC,  NC,   // | 01 : 3.3V     || 02 : 5.0V     |
         NC,  NC,   // | 03 : GPIO3_B6 || 04 : 5.0V     |
         NC,  NC,   // | 05 : GPIO3_B5 || 06 : GND      |
          1,   0,   // | 07 : GPIO0_B6 || 08 : GPIO3_D6 |
         NC,   1,   // | 09 : GND      || 10 : GPIO3_D7 |
          0,   1,   // | 11 : GPIO0_C0 || 12 : GPIO3_D0 |
          1,  NC,   // | 13 : GPIO0_C1 || 14 : GND      |
          0,   1,   // | 15 : GPIO3_B2 || 16 : GPIO3_C6 |
         NC,   0,   // | 17 : 3.3V     || 18 : GPIO3_C7 |
          1,  NC,   // | 19 : GPIO2_D1 || 20 : GND      |
          0,   1,   // | 21 : GPIO2_D0 || 22 : GPIO3_D1 |
          1,   0,   // | 23 : GPIO2_D3 || 24 : GPIO2_D2 |
         NC,   1,   // | 25 : GND      || 26 : GPIO3_D2 |
          1,   0,   // | 27 : GPIO0_B4 || 28 : GPIO0_B3 |
          0,  NC,   // | 29 : GPIO4_C1 || 30 : GND      |
          1,   0,   // | 31 : GPIO4_B6 || 32 : GPIO3_D3 |
          0,  NC,   // | 33 : GPIO0_B5 || 34 : GND      |
          1,   0,   // | 35 : GPIO3_D5 || 36 : GPIO3_D4 |
         NC,  NC,   // | 37 : ADC.AIN4 || 38 : 1.8V     |
         NC,  NC,   // | 39 : GND      || 40 : ADC.AIN5 |
    },
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int pattern_write (int pattern)
{
    int i;
    if (pattern < PATTERN_COUNT) {
        for (i = 0; i < (int)(sizeof(HEADER40)/sizeof(int)); i++) {
            if (HEADER40[i]) {
                gpio_set_value (HEADER40[i], H40_PATTERN[pattern][i]);
            }
        }
        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int header_pattern_set (int id)
{
    // header14, header40 pattern test
    return pattern_write (id);
}

//------------------------------------------------------------------------------
int header_pattern_check (int id, int *pattern40)
{
    int i;
    if (id < PATTERN_COUNT) {
        for (i = 0; i < (int)(sizeof(HEADER40)/sizeof(int)); i++) {
            if (HEADER40[i]) {
                if (H40_PATTERN[id][i]){
                    if (pattern40[i] < 3000) {
                        printf ("PT%d GPIO - %d : %d %dmV\n",
                            id, HEADER40[i], H40_PATTERN[id][i], pattern40[i]);
                        return 0;
                    }
                } else {
                    if (pattern40[i] > 300) {
                        printf ("PT%d GPIO - %d : %d %dmV\n",
                            id, HEADER40[i], H40_PATTERN[id][i], pattern40[i]);
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

//------------------------------------------------------------------------------
int header_init (void)
{
    int i;

    for (i = 0; i < (int)(sizeof(HEADER40)/sizeof(int)); i++) {
        if (HEADER40[i]) {
            gpio_export    (HEADER40[i]);
            gpio_direction (HEADER40[i], GPIO_DIR_OUT);

        }
    }

    return 1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
