//------------------------------------------------------------------------------
/**
 * @file adc.c
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "adc.h"

//------------------------------------------------------------------------------
// default adc range (mV). ADC res 1.7578125mV (1800mV / 1024 bits)
// adc voltage = adc raw read * ADC res (1.75)
// const 1.358V
#define DEFAULT_ADC_H37_H   1400
#define DEFAULT_ADC_H37_L   1340

// const 0.441V
#define DEFAULT_ADC_H40_H   490
#define DEFAULT_ADC_H40_L   430

//------------------------------------------------------------------------------
//
// Configuration
//
//------------------------------------------------------------------------------
#define STR_PATH_LENGTH 128

struct device_adc {
    // Control path
    char path[STR_PATH_LENGTH +1];
    // compare value(max, min mv)
    int max, min;

    // read value
    int value;
};

//------------------------------------------------------------------------------
/* define adc devices */
//------------------------------------------------------------------------------
struct device_adc DeviceADC [eADC_END] = {
    // eADC_H37 (Header 37) - const 1.358V
    { "/sys/bus/iio/devices/iio:device0/in_voltage4_raw", DEFAULT_ADC_H37_H, DEFAULT_ADC_H37_L, 0 },
    // eADC_H40 (Header 40) - const 0.441V
    { "/sys/bus/iio/devices/iio:device0/in_voltage5_raw", DEFAULT_ADC_H40_H, DEFAULT_ADC_H40_L, 0 },
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static int adc_read (const char *path)
{
    char rdata[16];
    int mV = 0;
    FILE *fp;

    memset (rdata, 0, sizeof (rdata));
    // adc raw value get
    if ((fp = fopen(path, "r")) != NULL) {
        fgets (rdata, sizeof(rdata), fp);
        fclose(fp);
    }
    mV = (atoi(rdata) * 1800) / 4096;
    return mV;
}

//------------------------------------------------------------------------------
int adc_check (int id)
{
    int value = 0;

    if (access (DeviceADC[id].path, R_OK) != 0)
        return 0;

    value = adc_read (DeviceADC[id].path);
    if ((value < DeviceADC[id].max) && (value > DeviceADC[id].min))
        return value;

    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
