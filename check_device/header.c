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
// ODROID-M1S Header GPIOs Define
//
//------------------------------------------------------------------------------
// Not Control
#define NC  0

const int HEADER14[] = {
    // Header J3 GPIOs
     NC,        // Not used (pin 0)
     NC,  NC,   // | 01 : DC_JACK  || 02 : 3.3V      |
     NC,  NC,   // | 03 : DC_JACK  || 04 : HP_R      |
     NC,  NC,   // | 05 : HP_DET   || 06 : GND       |
     NC,  NC,   // | 07 : MIC_IN_P || 08 : HP_L      |
     NC,  NC,   // | 09 : PWRBTN   || 10 : RESET_H   |
     NC,  NC,   // | 11 : GPIO3_C4 || 12 : GPIO3_C5  | -> GPIO I2C(ADC Board)
    118,  NC,   // | 13 : GPIO3_C6 || 14 : GND       |
};

const int HEADER40[] = {
    // Header J2 GPIOs
     NC,        // Not used (pin 0)
     NC,  NC,   // | 01 : 3.3V     || 02 : 5.0V     |
    135,  NC,   // | 03 : GPIO4_A7 || 04 : 5.0V     |
    134,  NC,   // | 05 : GPIO4_A6 || 06 : GND      |
     24, 112,   // | 07 : GPIO0_D0 || 08 : GPIO3_C0 |
     NC, 113,   // | 09 : GND      || 10 : GPIO3_C1 |
    124, 106,   // | 11 : GPIO3_D4 || 12 : GPIO3_B2 |
    125,  NC,   // | 13 : GPIO3_D5 || 14 : GND      |
    139,  42,   // | 15 : GPIO4_B3 || 16 : GPIO1_B2 |
     NC,  43,   // | 17 : 3.3V     || 18 : GPIO1_B3 |
    122,  NC,   // | 19 : GPIO3_D2 || 20 : GND      |
    121,  28,   // | 21 : GPIO3_D1 || 22 : GPIO0_D4 |
    123,  29,   // | 23 : GPIO3_D3 || 24 : GPIO0_D5 |
     NC,  44,   // | 25 : GND      || 26 : GPIO1_B4 |
    136, 137,   // | 27 : GPIO4_B0 || 28 : GPIO4_B1 |
     47,  NC,   // | 29 : GPIO1_B7 || 30 : GND      |
     46, 140,   // | 31 : GPIO1_B6 || 32 : GPIO4_B4 |
    120,  NC,   // | 33 : GPIO3_D0 || 34 : GND      |
    119, 141,   // | 35 : GPIO3_C7 || 36 : GPIO4_B5 |
     NC,  NC,   // | 37 : ADC.AIN4 || 38 : 1.8V     |
     NC,  NC,   // | 39 : GND      || 40 : ADC.AIN5 |
};

//------------------------------------------------------------------------------
#define PATTERN_COUNT   4

const int H14_PATTERN[PATTERN_COUNT][sizeof(HEADER14)] = {
    // Pattern 0 : ALL High
    {
        // Header J3 GPIOs
        NC,     // Not used (pin 0)
        NC, NC, // | 01 : DC_JACK  || 02 : 3.3V      |
        NC, NC, // | 03 : DC_JACK  || 04 : HP_R      |
        NC, NC, // | 05 : HP_DET   || 06 : GND       |
        NC, NC, // | 07 : MIC_IN_P || 08 : HP_L      |
        NC, NC, // | 09 : PWRBTN   || 10 : RESET_H   |
        NC, NC, // | 11 : GPIO3_C4 || 12 : GPIO3_C5  |
         1, NC, // | 13 : GPIO3_C6 || 14 : GND       |
    },
    // Pattern 1 : ALL Low
    {
        // Header J3 GPIOs
        NC,     // Not used (pin 0)
        NC, NC, // | 01 : DC_JACK  || 02 : 3.3V      |
        NC, NC, // | 03 : DC_JACK  || 04 : HP_R      |
        NC, NC, // | 05 : HP_DET   || 06 : GND       |
        NC, NC, // | 07 : MIC_IN_P || 08 : HP_L      |
        NC, NC, // | 09 : PWRBTN   || 10 : RESET_H   |
        NC, NC, // | 11 : GPIO3_C4 || 12 : GPIO3_C5  |
         0, NC, // | 13 : GPIO3_C6 || 14 : GND       |
    },
    // Pattern 2 : Cross 0
    {
        // Header J3 GPIOs
        NC,     // Not used (pin 0)
        NC, NC, // | 01 : DC_JACK  || 02 : 3.3V      |
        NC, NC, // | 03 : DC_JACK  || 04 : HP_R      |
        NC, NC, // | 05 : HP_DET   || 06 : GND       |
        NC, NC, // | 07 : MIC_IN_P || 08 : HP_L      |
        NC, NC, // | 09 : PWRBTN   || 10 : RESET_H   |
        NC, NC, // | 11 : GPIO3_C4 || 12 : GPIO3_C5  |
         1, NC, // | 13 : GPIO3_C6 || 14 : GND       |
    },
    // Pattern 3 : Cross 1
    {
        // Header J3 GPIOs
        NC,     // Not used (pin 0)
        NC, NC, // | 01 : DC_JACK  || 02 : 3.3V      |
        NC, NC, // | 03 : DC_JACK  || 04 : HP_R      |
        NC, NC, // | 05 : HP_DET   || 06 : GND       |
        NC, NC, // | 07 : MIC_IN_P || 08 : HP_L      |
        NC, NC, // | 09 : PWRBTN   || 10 : RESET_H   |
        NC, NC, // | 11 : GPIO3_C4 || 12 : GPIO3_C5  |
         0, NC, // | 13 : GPIO3_C6 || 14 : GND       |
    },
};

const int H40_PATTERN[PATTERN_COUNT][sizeof(HEADER40)] = {
    // Pattern 0 : ALL High
    {
        // Header J2 GPIOs
        NC,       // Not used (pin 0)
        NC, NC,   // | 01 : 3.3V     || 02 : 5.0V     |
         1, NC,   // | 03 : GPIO4_A7 || 04 : 5.0V     |
         1, NC,   // | 05 : GPIO4_A6 || 06 : GND      |
         1,  1,   // | 07 : GPIO0_D0 || 08 : GPIO3_C0 |
        NC,  1,   // | 09 : GND      || 10 : GPIO3_C1 |
         1,  1,   // | 11 : GPIO3_D4 || 12 : GPIO3_B2 |
         1, NC,   // | 13 : GPIO3_D5 || 14 : GND      |
         1,  1,   // | 15 : GPIO4_B3 || 16 : GPIO1_B2 |
        NC,  1,   // | 17 : 3.3V     || 18 : GPIO1_B3 |
         1, NC,   // | 19 : GPIO3_D2 || 20 : GND      |
         1,  1,   // | 21 : GPIO3_D1 || 22 : GPIO0_D4 |
         1,  1,   // | 23 : GPIO3_D3 || 24 : GPIO0_D5 |
        NC,  1,   // | 25 : GND      || 26 : GPIO1_B4 |
         1,  1,   // | 27 : GPIO4_B0 || 28 : GPIO4_B1 |
         1, NC,   // | 29 : GPIO1_B7 || 30 : GND      |
         1,  1,   // | 31 : GPIO1_B6 || 32 : GPIO4_B4 |
         1, NC,   // | 33 : GPIO3_D0 || 34 : GND      |
         1,  1,   // | 35 : GPIO3_C7 || 36 : GPIO4_B5 |
        NC, NC,   // | 37 : ADC.AIN4 || 38 : 1.8V     |
        NC, NC,   // | 39 : GND      || 40 : ADC.AIN5 |
    },
    // Pattern 1 : ALL Low
    {
        // Header J2 GPIOs
        NC,       // Not used (pin 0)
        NC, NC,   // | 01 : 3.3V     || 02 : 5.0V     |
         0, NC,   // | 03 : GPIO4_A7 || 04 : 5.0V     |
         0, NC,   // | 05 : GPIO4_A6 || 06 : GND      |
         0,  0,   // | 07 : GPIO0_D0 || 08 : GPIO3_C0 |
        NC,  0,   // | 09 : GND      || 10 : GPIO3_C1 |
         0,  0,   // | 11 : GPIO3_D4 || 12 : GPIO3_B2 |
         0, NC,   // | 13 : GPIO3_D5 || 14 : GND      |
         0,  0,   // | 15 : GPIO4_B3 || 16 : GPIO1_B2 |
        NC,  0,   // | 17 : 3.3V     || 18 : GPIO1_B3 |
         0, NC,   // | 19 : GPIO3_D2 || 20 : GND      |
         0,  0,   // | 21 : GPIO3_D1 || 22 : GPIO0_D4 |
         0,  0,   // | 23 : GPIO3_D3 || 24 : GPIO0_D5 |
        NC,  0,   // | 25 : GND      || 26 : GPIO1_B4 |
         0,  0,   // | 27 : GPIO4_B0 || 28 : GPIO4_B1 |
         0, NC,   // | 29 : GPIO1_B7 || 30 : GND      |
         0,  0,   // | 31 : GPIO1_B6 || 32 : GPIO4_B4 |
         0, NC,   // | 33 : GPIO3_D0 || 34 : GND      |
         0,  0,   // | 35 : GPIO3_C7 || 36 : GPIO4_B5 |
        NC, NC,   // | 37 : ADC.AIN4 || 38 : 1.8V     |
        NC, NC,   // | 39 : GND      || 40 : ADC.AIN5 |
    },
    // Pattern 2 : Cross 0
    {
        // Header J2 GPIOs
        NC,       // Not used (pin 0)
        NC, NC,   // | 01 : 3.3V     || 02 : 5.0V     |
         0, NC,   // | 03 : GPIO4_A7 || 04 : 5.0V     |
         1, NC,   // | 05 : GPIO4_A6 || 06 : GND      |
         0,  1,   // | 07 : GPIO0_D0 || 08 : GPIO3_C0 |
        NC,  0,   // | 09 : GND      || 10 : GPIO3_C1 |
         0,  1,   // | 11 : GPIO3_D4 || 12 : GPIO3_B2 |
         1, NC,   // | 13 : GPIO3_D5 || 14 : GND      |
         0,  1,   // | 15 : GPIO4_B3 || 16 : GPIO1_B2 |
        NC,  0,   // | 17 : 3.3V     || 18 : GPIO1_B3 |
         1, NC,   // | 19 : GPIO3_D2 || 20 : GND      |
         0,  1,   // | 21 : GPIO3_D1 || 22 : GPIO0_D4 |
         1,  0,   // | 23 : GPIO3_D3 || 24 : GPIO0_D5 |
        NC,  1,   // | 25 : GND      || 26 : GPIO1_B4 |
         1,  0,   // | 27 : GPIO4_B0 || 28 : GPIO4_B1 |
         0, NC,   // | 29 : GPIO1_B7 || 30 : GND      |
         1,  0,   // | 31 : GPIO1_B6 || 32 : GPIO4_B4 |
         0, NC,   // | 33 : GPIO3_D0 || 34 : GND      |
         1,  0,   // | 35 : GPIO3_C7 || 36 : GPIO4_B5 |
        NC, NC,   // | 37 : ADC.AIN4 || 38 : 1.8V     |
        NC, NC,   // | 39 : GND      || 40 : ADC.AIN5 |
    },
    // Pattern 3 : Cross 1
    {
        // Header J2 GPIOs
        NC,       // Not used (pin 0)
        NC, NC,   // | 01 : 3.3V     || 02 : 5.0V     |
         1, NC,   // | 03 : GPIO4_A7 || 04 : 5.0V     |
         0, NC,   // | 05 : GPIO4_A6 || 06 : GND      |
         1,  0,   // | 07 : GPIO0_D0 || 08 : GPIO3_C0 |
        NC,  1,   // | 09 : GND      || 10 : GPIO3_C1 |
         1,  0,   // | 11 : GPIO3_D4 || 12 : GPIO3_B2 |
         0, NC,   // | 13 : GPIO3_D5 || 14 : GND      |
         1,  0,   // | 15 : GPIO4_B3 || 16 : GPIO1_B2 |
        NC,  1,   // | 17 : 3.3V     || 18 : GPIO1_B3 |
         0, NC,   // | 19 : GPIO3_D2 || 20 : GND      |
         1,  0,   // | 21 : GPIO3_D1 || 22 : GPIO0_D4 |
         0,  1,   // | 23 : GPIO3_D3 || 24 : GPIO0_D5 |
        NC,  0,   // | 25 : GND      || 26 : GPIO1_B4 |
         0,  1,   // | 27 : GPIO4_B0 || 28 : GPIO4_B1 |
         1, NC,   // | 29 : GPIO1_B7 || 30 : GND      |
         0,  1,   // | 31 : GPIO1_B6 || 32 : GPIO4_B4 |
         1, NC,   // | 33 : GPIO3_D0 || 34 : GND      |
         0,  1,   // | 35 : GPIO3_C7 || 36 : GPIO4_B5 |
        NC, NC,   // | 37 : ADC.AIN4 || 38 : 1.8V     |
        NC, NC,   // | 39 : GND      || 40 : ADC.AIN5 |
    },
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int pattern_write (int pattern)
{
    if (pattern < PATTERN_COUNT) {
        int i;
        for (i = 0; i < (int)(sizeof(HEADER14)/sizeof(int)); i++) {
            if (HEADER14[i]) {
                gpio_set_value (HEADER14[i], H14_PATTERN[pattern][i]);
            }
        }
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
int header_pattern_check (int id, int *pattern40, int *pattern14)
{
    int i;
    if (id < PATTERN_COUNT) {
        for (i = 0; i < (int)(sizeof(HEADER14)/sizeof(int)); i++) {
            if (HEADER14[i]) {
                if (H14_PATTERN[id][i]){
                    if (pattern14[i] < 3000) {
                        printf ("PT%d GPIO - %d : %d %dmV\n",
                            id, HEADER14[i], H14_PATTERN[id][i], pattern14[i]);
                        return 0;
                    }
                } else {
                    if (pattern14[i] > 300) {
                        printf ("PT%d GPIO - %d : %d %dmV\n",
                            id, HEADER14[i], H14_PATTERN[id][i], pattern14[i]);
                        return 0;
                    }
                }
            }
        }
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

    for (i = 0; i < (int)(sizeof(HEADER14)/sizeof(int)); i++) {
        if (HEADER14[i]) {
            gpio_export    (HEADER14[i]);
            gpio_direction (HEADER14[i], GPIO_DIR_OUT);
        }
    }

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
