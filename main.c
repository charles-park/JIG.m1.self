//------------------------------------------------------------------------------
/**
 * @file client.c
 * @author charles-park (charles.park@hardkernel.com)
 * @brief ODROID-M1 JIG Self-test App.
 * @version 0.2
 * @date 2024-09-11
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
#include <sys/time.h>
#include <linux/fb.h>
#include <getopt.h>
#include <pthread.h>

#include <signal.h>

//------------------------------------------------------------------------------
#include "lib_fbui/lib_fb.h"
#include "lib_fbui/lib_ui.h"
#include "nlp_server_ctrl/nlp_server_ctrl.h"
#include "lib_mac/lib_mac.h"
#include "lib_efuse/lib_efuse.h"
#include "lib_gpio/lib_gpio.h"
#include "lib_i2cadc/lib_i2cadc.h"

#include "check_device/adc.h"
#include "check_device/hdmi.h"
#include "check_device/system.h"
#include "check_device/storage.h"
#include "check_device/usb.h"
#include "check_device/ethernet.h"
#include "check_device/led.h"
#include "check_device/header.h"
#include "check_device/audio.h"

//------------------------------------------------------------------------------
//
// JIG Protocol(V2.0)
// https://docs.google.com/spreadsheets/d/1Of7im-2I5m_M-YKswsubrzQAXEGy-japYeH8h_754WA/edit#gid=0
//
//------------------------------------------------------------------------------
#define DEVICE_FB   "/dev/fb0"
#define CONFIG_UI   "m1.cfg"

#define ALIVE_DISPLAY_UI_ID     0
#define ALIVE_DISPLAY_INTERVAL  1000

#define APP_LOOP_DELAY  500

#define TIMEOUT_SEC     60

#define IP_ADDR_SIZE    20

#define TEST_MODEL_NONE 0
#define TEST_MODEL_8GB  8
#define TEST_MODEL_16GB 16

//------------------------------------------------------------------------------
static int TimeoutStop = TIMEOUT_SEC;

//------------------------------------------------------------------------------
typedef struct client__t {
    // HDMI UI
    fb_info_t   *pfb;
    ui_grp_t    *pui;

    int adc_fd;
    int channel;
    int test_model;     // 0 : none, 1 : 8GB, 2 : 16GB (ADC P3.9->16GB, ADC P3.8->8GB)
    int board_mem;
    int eth_switch;     // 0 : stop, 1 : running

    char nlp_ip     [IP_ADDR_SIZE];
    char efuse_data [EFUSE_UUID_SIZE +1];
    char mac        [MAC_STR_SIZE +1];
}   client_t;

//------------------------------------------------------------------------------
enum { eRESULT_FAIL = 0, eRESULT_PASS };

enum {
    eSTATUS_WAIT = 0,
    eSTATUS_RUN,
    eSTATUS_STOP,
    eSTATUS_END
};

enum {
    eITEM_BOARD_IP = 0,
    eITEM_SERVER_IP,

    // system
    eITEM_FB,
    eITEM_MEM,

    // hdmi
    eITEM_EDID,
    eITEM_HPD,

    eITEM_IPERF,
    eITEM_MAC_ADDR,

    // adc
    eITEM_ADC37,
    eITEM_ADC40,

    // storage
    eITEM_eMMC,
    eITEM_uSD,
    eITEM_NVME,

    // usb
    eITEM_USB30,
    eITEM_USB20,
    eITEM_USB_C,

    eITEM_SW_eMMC,
    eITEM_SW_uSD,

    eITEM_ETHERNET_100M,
    eITEM_ETHERNET_1G,

    eITEM_HEADER_PT1,
    eITEM_HEADER_PT2,
    eITEM_HEADER_PT3,
    eITEM_HEADER_PT4,

    // HP_DETECT
    eITEM_HP_DET_L,
    eITEM_HP_DET_H,

    eITEM_AUDIO_LEFT,
    eITEM_AUDIO_RIGHT,

    eITEM_END
};

enum {
    eUI_BOARD_IP = 4,
    eUI_SERVER_IP = 24,
    eUI_FB = 52,
    eUI_MEM = 8,
    eUI_EDID = 53,
    eUI_HPD = 54,
    eUI_IPERF = 127,
    eUI_MAC_ADDR = 147,
    eUI_ADC37 = 192,
    eUI_ADC40 = 193,
    eUI_eMMC = 62,
    eUI_uSD = 82,
    eUI_NVME = 87,
    eUI_USB30 = 102,
    eUI_USB20 = 107,
    eUI_USB_C = 122,
    eUI_SW_eMMC = 178,
    eUI_SW_uSD = 179,
    eUI_ETHERNET_100M = 152,
    eUI_ETHERNET_1G = 153,
    eUI_HEADER_PT1 = 172,
    eUI_HEADER_PT2 = 173,
    eUI_HEADER_PT3 = 174,
    eUI_HEADER_PT4 = 175,
    eUI_HP_DET_H = 198,
    eUI_HP_DET_L = 199,
    eUI_AUDIO_LEFT = 196,
    eUI_AUDIO_RIGHT = 197,
    eUI_END
};

struct check_item {
    int id, ui_id, status, result;
    const char *name;
};

struct check_item m2_item [eITEM_END] = {
    { eITEM_BOARD_IP,       eUI_BOARD_IP,       eSTATUS_WAIT, eRESULT_FAIL, "bip" },
    { eITEM_SERVER_IP,      eUI_SERVER_IP,      eSTATUS_WAIT, eRESULT_FAIL, "sip" },
    { eITEM_FB,             eUI_FB,             eSTATUS_WAIT, eRESULT_FAIL, "fb" },
    { eITEM_MEM,            eUI_MEM,            eSTATUS_WAIT, eRESULT_FAIL, "mem" },
    { eITEM_EDID,           eUI_EDID,           eSTATUS_WAIT, eRESULT_FAIL, "edid" },
    { eITEM_HPD,            eUI_HPD,            eSTATUS_WAIT, eRESULT_FAIL, "hpd" },
    { eITEM_IPERF,          eUI_IPERF,          eSTATUS_WAIT, eRESULT_FAIL, "iperf" },
    { eITEM_MAC_ADDR,       eUI_MAC_ADDR,       eSTATUS_WAIT, eRESULT_FAIL, "mac" },
    { eITEM_ADC37,          eUI_ADC37,          eSTATUS_WAIT, eRESULT_FAIL, "adc37" },
    { eITEM_ADC40,          eUI_ADC40,          eSTATUS_WAIT, eRESULT_FAIL, "adc40" },
    { eITEM_eMMC,           eUI_eMMC,           eSTATUS_WAIT, eRESULT_FAIL, "emmc" },
    { eITEM_uSD,            eUI_uSD,            eSTATUS_WAIT, eRESULT_FAIL, "sd" },
    { eITEM_NVME,           eUI_NVME,           eSTATUS_WAIT, eRESULT_FAIL, "nvme" },
    { eITEM_USB30,          eUI_USB30,          eSTATUS_WAIT, eRESULT_FAIL, "usb3" },
    { eITEM_USB20,          eUI_USB20,          eSTATUS_WAIT, eRESULT_FAIL, "usb2" },
    { eITEM_USB_C,          eUI_USB_C,          eSTATUS_WAIT, eRESULT_FAIL, "usbc" },
    { eITEM_SW_eMMC,        eUI_SW_eMMC,        eSTATUS_WAIT, eRESULT_FAIL, "sw-e" },
    { eITEM_SW_uSD,         eUI_SW_uSD,         eSTATUS_WAIT, eRESULT_FAIL, "sw-s" },
    { eITEM_ETHERNET_100M,  eUI_ETHERNET_100M,  eSTATUS_WAIT, eRESULT_FAIL, "eth-l" },
    { eITEM_ETHERNET_1G,    eUI_ETHERNET_1G,    eSTATUS_WAIT, eRESULT_FAIL, "eth-h" },
    { eITEM_HEADER_PT1,     eUI_HEADER_PT1,     eSTATUS_WAIT, eRESULT_FAIL, "h1" },
    { eITEM_HEADER_PT2,     eUI_HEADER_PT2,     eSTATUS_WAIT, eRESULT_FAIL, "h2" },
    { eITEM_HEADER_PT3,     eUI_HEADER_PT3,     eSTATUS_WAIT, eRESULT_FAIL, "h3" },
    { eITEM_HEADER_PT4,     eUI_HEADER_PT4,     eSTATUS_WAIT, eRESULT_FAIL, "h4" },
    { eITEM_HP_DET_L,       eUI_HP_DET_L,       eSTATUS_WAIT, eRESULT_FAIL, "det-l" },
    { eITEM_HP_DET_H,       eUI_HP_DET_H,       eSTATUS_WAIT, eRESULT_FAIL, "det-h" },
    { eITEM_AUDIO_LEFT,     eUI_AUDIO_LEFT,     eSTATUS_WAIT, eRESULT_FAIL, "a-l" },
    { eITEM_AUDIO_RIGHT,    eUI_AUDIO_RIGHT,    eSTATUS_WAIT, eRESULT_FAIL, "a-r" },
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// 문자열 변경 함수. 입력 포인터는 반드시 메모리가 할당되어진 변수여야 함.
//------------------------------------------------------------------------------
static void tolowerstr (char *p)
{
    int i, c = strlen(p);

    for (i = 0; i < c; i++, p++)
        *p = tolower(*p);
}

//------------------------------------------------------------------------------
static void toupperstr (char *p)
{
    int i, c = strlen(p);

    for (i = 0; i < c; i++, p++)
        *p = toupper(*p);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define	PRINT_MAX_CHAR	50
#define	PRINT_MAX_LINE	2

int errcode_print (client_t *p)
{
    char err_msg[PRINT_MAX_LINE][PRINT_MAX_CHAR+1];
    int pos = 0, i, line;

    memset (err_msg, 0, sizeof(err_msg));

    for (i = 0, line = 0; i < eITEM_END; i++) {
        if (!m2_item[i].result) {
            if ((pos + strlen(m2_item[i].name) + 1) > PRINT_MAX_CHAR) {
                pos = 0, line++;
            }
            pos += sprintf (&err_msg[line][pos], "%s,", m2_item[i].name);
            ui_set_ritem (p->pfb, p->pui, m2_item [i].ui_id, COLOR_RED, -1);
        }
    }
    if (pos || line) {
        for (i = 0; i < line+1; i++) {
            nlp_server_write (p->nlp_ip, NLP_SERVER_MSG_TYPE_ERR, &err_msg[i][0], 0);
            printf ("%s : msg = %s\n", __func__, &err_msg[i][0]);
        }
        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
#define UI_STATUS   47
#define	RUN_BOX_ON	RGB_TO_UINT(204, 204, 0)
#define	RUN_BOX_OFF	RGB_TO_UINT(153, 153, 0)

void *check_status (void *arg);
void *check_status (void *arg)
{
    static int onoff = 0, err = 0;
    char str [16];
    client_t *p = (client_t *)arg;

    while (TimeoutStop) {
        ui_set_ritem (p->pfb, p->pui, ALIVE_DISPLAY_UI_ID,
                    onoff ? COLOR_GREEN : p->pui->bc.uint, -1);
        onoff = !onoff;

        if (m2_item[eITEM_SERVER_IP].result && TimeoutStop) {
            memset (str, 0, sizeof(str));
            if (p->adc_fd != -1) {
                ui_set_ritem (p->pfb, p->pui, UI_STATUS, onoff ? RUN_BOX_ON : RUN_BOX_OFF, -1);
                sprintf (str, "RUNNING %d", TimeoutStop);
            } else {
                ui_set_ritem (p->pfb, p->pui, UI_STATUS, onoff ? COLOR_RED : p->pui->bc.uint, -1);
                sprintf (str, "I2CADC %d", TimeoutStop);
            }
            ui_set_sitem (p->pfb, p->pui, UI_STATUS, -1, -1, str);
        }
        if (onoff) {
            ui_update (p->pfb, p->pui, -1);
            if (TimeoutStop && (p->adc_fd != -1))   TimeoutStop--;
        }

        led_set_status (eLED_POWER,  onoff);
        led_set_status ( eLED_ALIVE, onoff);
        usleep (APP_LOOP_DELAY * 1000);
        {
            int stop_cnt = 0, i;

            for (i = 0; i < eITEM_END; i++) {
                if (m2_item[i].status == eSTATUS_STOP) stop_cnt++;
            }

            if (stop_cnt == eITEM_END) {
                TimeoutStop = 0;    break;
            }
        }
    }

    // ethernet switch thread check.
    while (p->eth_switch)   usleep (APP_LOOP_DELAY * 1000);

    // display stop
    memset (str, 0, sizeof(str));   sprintf (str, "%s", "FINISH");
    ethernet_link_setup (LINK_SPEED_1G);
    // wait for network stable
    usleep (APP_LOOP_DELAY * 1000);

    if (m2_item [eITEM_MAC_ADDR].result)
        nlp_server_write (p->nlp_ip, NLP_SERVER_MSG_TYPE_MAC, p->mac, p->channel);
    ui_set_sitem (p->pfb, p->pui, UI_STATUS, -1, -1, str);
    err = errcode_print (p);
    ui_set_ritem (p->pfb, p->pui, UI_STATUS, err ? COLOR_RED : COLOR_GREEN, -1);

    while (1) {
        usleep (APP_LOOP_DELAY * 1000);
        onoff = !onoff;

        if (onoff)
            ui_set_ritem (p->pfb, p->pui, UI_STATUS, err ? COLOR_RED : COLOR_GREEN, -1);
        else
            ui_set_ritem (p->pfb, p->pui, UI_STATUS, p->pui->bc.uint, -1);
        ui_update    (p->pfb, p->pui, -1);
    }
    return arg;
}

//------------------------------------------------------------------------------
#define HP_DET_GPIO 61

void *check_hp_detect (void *arg);
void *check_hp_detect (void *arg)
{
    int value = 0, new_value = 0, long_press_cnt = 0;

    client_t *p = (client_t *)arg;

    gpio_export    (HP_DET_GPIO);
    gpio_direction (HP_DET_GPIO, GPIO_DIR_IN);
    gpio_get_value (HP_DET_GPIO, &value);

    m2_item[eITEM_HP_DET_H].status = m2_item[eITEM_HP_DET_L].status = eSTATUS_RUN;
    while (1) {
        if (gpio_get_value (HP_DET_GPIO, &new_value)) {

            if (value != new_value) {
                value = new_value;
                if (value) {
                    if (m2_item[eITEM_HP_DET_H].status == eSTATUS_RUN) {
                        ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_HP_DET_H].ui_id, -1, -1, "PASS");
                        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_HP_DET_H].ui_id, COLOR_GREEN, -1);
                        m2_item[eITEM_HP_DET_H].result = eRESULT_PASS;
                        m2_item[eITEM_HP_DET_H].status = eSTATUS_STOP;
                    }
                } else {
                    if (m2_item[eITEM_HP_DET_L].status == eSTATUS_RUN) {
                        ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_HP_DET_L].ui_id, -1, -1, "PASS");
                        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_HP_DET_L].ui_id, COLOR_GREEN, -1);
                        m2_item[eITEM_HP_DET_L].result = eRESULT_PASS;
                        m2_item[eITEM_HP_DET_L].status = eSTATUS_STOP;
                    }
                }
            }
        }
        usleep (100 * 1000);

        if (new_value)  long_press_cnt++;
        else            long_press_cnt = 0;

        if (long_press_cnt > 30) {
            long_press_cnt = 0;
            if (m2_item [eITEM_MAC_ADDR].result) {
                tolowerstr (p->mac);
                nlp_server_write (p->nlp_ip, NLP_SERVER_MSG_TYPE_MAC, p->mac, p->channel);
            }
        }
    }
    gpio_unexport  (HP_DET_GPIO);
    return arg;
}

//------------------------------------------------------------------------------
#define SW_ADC_PATH "/sys/bus/iio/devices/iio:device0/in_voltage0_raw"

// emmc : 1380 - 1390 - 1400
// sd : 680 - 690 - 700
int sw_adc_read (const char *path)
{
    char rdata[16];
    FILE *fp;

    memset (rdata, 0, sizeof (rdata));
    // adc raw value get
    if ((fp = fopen(path, "r")) != NULL) {
        fgets (rdata, sizeof(rdata), fp);
        fclose(fp);
    }
    return atoi(rdata);
}

#define SW_eMMC_MIN 1380
#define SW_eMMC_MAX 1400
#define SW_uSD_MIN  680
#define SW_uSD_MAX  700

static int check_i2cadc (client_t *p);

void *check_sw_adc (void *arg);
void *check_sw_adc (void *arg)
{
    int value = 0, new_value = 0, status = 0, adc_value = 0;

    client_t *p = (client_t *)arg;

    while (1) {
	usleep (100 * 1000);
        adc_value = sw_adc_read (SW_ADC_PATH);
        if ((SW_eMMC_MIN < adc_value) && (SW_eMMC_MAX > adc_value ))
        {   value = 0;  break; }

        if ((SW_uSD_MIN < adc_value) && (SW_uSD_MAX > adc_value ))
        {   value = 1;  break; }
        printf ("sw adc value error! (emmc:1380~1400, sd:680~700) : %d\n", adc_value);
    }
    new_value = value;
    m2_item[eITEM_SW_uSD].status = m2_item[eITEM_SW_eMMC].status = eSTATUS_RUN;
    while (TimeoutStop) {
        adc_value = sw_adc_read (SW_ADC_PATH);

        if ((SW_eMMC_MIN < adc_value) && (SW_eMMC_MAX > adc_value))
            new_value = 0;

        if ((SW_uSD_MIN < adc_value) && (SW_uSD_MAX > adc_value))
            new_value = 1;

        if (value != new_value) {
            value = new_value;
            status |= value ? 0x02 : 0x01;

            if (value) {
                m2_item[eITEM_SW_uSD].result = eRESULT_PASS;
                ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_SW_uSD].ui_id, -1, -1, "PASS");
                ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_SW_uSD].ui_id, COLOR_GREEN, -1);
            } else {
                m2_item[eITEM_SW_eMMC].result = eRESULT_PASS;
                ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_SW_eMMC].ui_id, -1, -1, "PASS");
                ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_SW_eMMC].ui_id, COLOR_GREEN, -1);
            }
        }

        usleep (100 * 1000);
        if (m2_item[eITEM_SW_uSD].result && m2_item[eITEM_SW_eMMC].result) break;
    }
    m2_item[eITEM_SW_uSD].status = m2_item[eITEM_SW_eMMC].status = eSTATUS_STOP;
    return arg;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define UI_ETHERNET_SWITCH  154

void *check_device_ethernet (void *arg)
{
    int speed;
    client_t *p = (client_t *)arg;

    m2_item[eITEM_ETHERNET_100M].status = m2_item[eITEM_ETHERNET_1G].status = eSTATUS_RUN;

    // ethernet switch thread run
    p->eth_switch = 1;

    while (TimeoutStop) {
        switch (speed) {
            case LINK_SPEED_1G:
                if (ethernet_link_setup (LINK_SPEED_100M)) {
                    m2_item[eITEM_ETHERNET_100M].status = eSTATUS_STOP;
                    m2_item[eITEM_ETHERNET_100M].result = eRESULT_PASS;
                    ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_ETHERNET_100M].ui_id, -1, -1, "PASS");
                    ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_ETHERNET_100M].ui_id, COLOR_GREEN, -1);
                }
                break;
            case LINK_SPEED_100M:
                if (ethernet_link_setup (LINK_SPEED_1G)) {
                    m2_item[eITEM_ETHERNET_1G].status = eSTATUS_STOP;
                    m2_item[eITEM_ETHERNET_1G].result = eRESULT_PASS;
                    ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_ETHERNET_1G].ui_id, -1, -1, "PASS");
                    ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_ETHERNET_1G].ui_id, COLOR_GREEN, -1);
                }
                break;
            default :
                break;
        }
        speed = ethernet_link_check ();

        if (m2_item [eITEM_ETHERNET_1G].result && m2_item [eITEM_ETHERNET_100M].result) {
            if (speed == LINK_SPEED_100M)
                ui_set_sitem (p->pfb, p->pui, UI_ETHERNET_SWITCH, -1, -1, "GREEN");
            else
                ui_set_sitem (p->pfb, p->pui, UI_ETHERNET_SWITCH, -1, -1, "ORANGE");

            ui_set_ritem (p->pfb, p->pui, UI_ETHERNET_SWITCH, RUN_BOX_ON, -1);
        }
        usleep (APP_LOOP_DELAY * 1000);
    }
    // ethernet switch thread end
    p->eth_switch = 0;
    return arg;
}

//------------------------------------------------------------------------------
void *check_device_usb (void *arg);
void *check_device_usb (void *arg)
{
    int value = 0;
    char str[10];
    client_t *p = (client_t *)arg;

    while (1) {
        // USB30
        if (!m2_item[eITEM_USB30].result) {
            m2_item[eITEM_USB30].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_USB30].ui_id, COLOR_YELLOW, -1);
            value = usb_check (eUSB_30);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);
            ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_USB30].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_USB30].ui_id, (value > 100) ? COLOR_GREEN : COLOR_RED, -1);
            m2_item[eITEM_USB30].result = (value > 100) ? eRESULT_PASS : eRESULT_FAIL;
            m2_item[eITEM_USB30].status = eSTATUS_STOP;
        }

        // USB20
        if (!m2_item[eITEM_USB20].result) {
            m2_item[eITEM_USB20].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_USB20].ui_id, COLOR_YELLOW, -1);
            value = usb_check (eUSB_20);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);

            ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_USB20].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_USB20].ui_id, (value > 25) ? COLOR_GREEN : COLOR_RED, -1);
            m2_item[eITEM_USB20].result = (value > 30) ? eRESULT_PASS : eRESULT_FAIL;
            m2_item[eITEM_USB20].status = eSTATUS_STOP;
        }

        // USB_C
        if (!m2_item[eITEM_USB_C].result) {
            m2_item[eITEM_USB_C].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_USB_C].ui_id, COLOR_YELLOW, -1);
            value = usb_check (eUSB_C);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);

            ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_USB_C].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_USB_C].ui_id, (value > 100) ? COLOR_GREEN : COLOR_RED, -1);
            m2_item[eITEM_USB_C].result = (value > 100) ? eRESULT_PASS : eRESULT_FAIL;
            m2_item[eITEM_USB_C].status = eSTATUS_STOP;
        }
        if (m2_item[eITEM_USB30].result && m2_item[eITEM_USB20].result && m2_item[eITEM_USB_C].result)
            break;
        usleep (APP_LOOP_DELAY * 1000);
    }

    return arg;
}

//------------------------------------------------------------------------------
static int check_header (client_t *p)
{
    static int init = 0;
    int ui_id = m2_item[eITEM_HEADER_PT1].ui_id, i;
    int pattern40[40 +1], pattern14[14 +1], cnt;

    if (!init)  {   header_init (); init = 1; }

    for (i = 0; i < eHEADER_END; i++) {
        if (!m2_item[eITEM_HEADER_PT1 + i].result) {
            ui_set_ritem (p->pfb, p->pui, ui_id + i, COLOR_YELLOW, -1);
            m2_item[eITEM_HEADER_PT1 + i].status = eSTATUS_RUN;
            header_pattern_set   (i);   usleep (APP_LOOP_DELAY * 1000);
            memset (pattern40, 0, sizeof(pattern40));
            memset (pattern14, 0, sizeof(pattern14));
            adc_board_read (p->adc_fd,  "CON1", &pattern40[1],  &cnt);
            adc_board_read (p->adc_fd, "P13.6", &pattern14[13], &cnt);
            if (header_pattern_check (i, pattern40, pattern14)) {
                m2_item[eITEM_HEADER_PT1 + i].result = eRESULT_PASS;
                ui_set_sitem (p->pfb, p->pui, ui_id + i, -1, -1, "PASS");
                ui_set_ritem (p->pfb, p->pui, ui_id + i, COLOR_GREEN, -1);
            } else {
                m2_item[eITEM_HEADER_PT1 + i].result = eRESULT_FAIL;
                ui_set_sitem (p->pfb, p->pui, ui_id + i, -1, -1, "FAIL");
                ui_set_ritem (p->pfb, p->pui, ui_id + i, COLOR_RED, -1);
            }
            m2_item[eITEM_HEADER_PT1 + i].status = eSTATUS_STOP;
        }
    }
    return 1;
}

//------------------------------------------------------------------------------
void *check_device_storage (void *arg);
void *check_device_storage (void *arg)
{
    int value = 0;
    char str[10];
    client_t *p = (client_t *)arg;

    while (1) {
        // eMMC
        if (!m2_item [eITEM_eMMC].result) {
            m2_item[eITEM_eMMC].status = eSTATUS_RUN;

            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_eMMC].ui_id, COLOR_YELLOW, -1);
            value = storage_check (eSTORAGE_eMMC);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);

            ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_eMMC].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_eMMC].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m2_item[eITEM_eMMC].result = value ? eRESULT_PASS : eRESULT_FAIL;

            if (m2_item[eITEM_eMMC].result) m2_item[eITEM_eMMC].status = eSTATUS_STOP;
        }

        // uSD
        if (!m2_item [eITEM_uSD].result) {
            m2_item[eITEM_uSD].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_uSD].ui_id, COLOR_YELLOW, -1);
            value = storage_check (eSTORAGE_uSD);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);

            ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_uSD].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_uSD].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m2_item[eITEM_uSD].result = value ? eRESULT_PASS : eRESULT_FAIL;

            if (m2_item[eITEM_uSD].result)  m2_item[eITEM_uSD].status = eSTATUS_STOP;
        }

        // NVME
        if (!m2_item [eITEM_NVME].result) {
            m2_item[eITEM_NVME].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_NVME].ui_id, COLOR_YELLOW, -1);
            value = storage_check (eSTORAGE_NVME);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);

            ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_NVME].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_NVME].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m2_item[eITEM_NVME].result = value ? eRESULT_PASS : eRESULT_FAIL;

            if (m2_item[eITEM_NVME].result) m2_item[eITEM_NVME].status = eSTATUS_STOP;
        }
        if (m2_item [eITEM_eMMC].result && m2_item [eITEM_uSD].result && m2_item [eITEM_NVME].result)
            break;
        usleep (APP_LOOP_DELAY * 1000);
    }
    return arg;
}

//------------------------------------------------------------------------------
static int check_device_system (client_t *p)
{
    int value = 0;
    char str[20];

    // MEM
//    if (!m2_item[eITEM_MEM].result && TimeoutStop) {
    if (TimeoutStop) {
        m2_item[eITEM_MEM].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_MEM].ui_id, COLOR_YELLOW, -1);
        value = system_check (eSYSTEM_MEM);
        p->board_mem = value;
        memset (str, 0, sizeof(str));
        if (p->test_model) {
            sprintf (str, "%d / T-%d GB", p->board_mem, p->test_model);
            ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_MEM].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_MEM].ui_id,
                            (p->test_model == p->board_mem) ? COLOR_GREEN : COLOR_RED, -1);
        } else {
            sprintf(str, "%d GB", value);
            ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_MEM].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_MEM].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m2_item[eITEM_MEM].result = value ? eRESULT_PASS : eRESULT_FAIL;
        }
        m2_item[eITEM_MEM].status = eSTATUS_STOP;
    }

    // FB
    if (!m2_item[eITEM_FB].result) {
        m2_item[eITEM_FB].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_FB].ui_id, COLOR_YELLOW, -1);
        value = system_check (eSYSTEM_FB_Y);
        memset (str, 0, sizeof(str));   sprintf(str, "%dP", value);

        ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_FB].ui_id, -1, -1, str);
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_FB].ui_id, (value == 1080) ? COLOR_GREEN : COLOR_RED, -1);
        m2_item[eITEM_FB].result = (value == 1080) ? eRESULT_PASS : eRESULT_FAIL;
        m2_item[eITEM_FB].status = eSTATUS_STOP;
    }

    if (p->test_model && (p->test_model != p->board_mem))
        m2_item[eITEM_MEM].result = eRESULT_FAIL;

    return 1;
}

//------------------------------------------------------------------------------
static int check_device_hdmi (client_t *p)
{
    int value = 0;

    // EDID
    if (!m2_item[eITEM_EDID].result) {
        m2_item[eITEM_EDID].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_EDID].ui_id, COLOR_YELLOW, -1);
        value = hdmi_check (eHDMI_EDID);
        ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_EDID].ui_id, -1, -1, value ? "PASS":"FAIL");
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_EDID].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
        m2_item[eITEM_EDID].result = value ? eRESULT_PASS : eRESULT_FAIL;
        m2_item[eITEM_EDID].status = eSTATUS_STOP;
    }

    // HPD
    if (!m2_item[eITEM_HPD].result) {
        m2_item[eITEM_HPD].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_HPD].ui_id, COLOR_YELLOW, -1);
        value = hdmi_check (eHDMI_HPD);
        ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_HPD].ui_id, -1, -1, value ? "PASS":"FAIL");
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_HPD].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
        m2_item[eITEM_HPD].result = value ? eRESULT_PASS : eRESULT_FAIL;
        m2_item[eITEM_HPD].status = eSTATUS_STOP;
    }

    return 1;
}

//------------------------------------------------------------------------------
static int check_device_adc (client_t *p)
{
    int adc_value = 0;
    char str[10];

    // ADC37
    if (!m2_item[eITEM_ADC37].result) {
        m2_item[eITEM_ADC37].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_ADC37].ui_id, COLOR_YELLOW, -1);
        adc_value = adc_check (eADC_H37);
        memset  (str, 0, sizeof(str));  sprintf (str, "%d", adc_value);
        ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_ADC37].ui_id, -1, -1, str);
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_ADC37].ui_id, adc_value ? COLOR_GREEN : COLOR_RED, -1);
        m2_item[eITEM_ADC37].result = adc_value ? eRESULT_PASS : eRESULT_FAIL;
        m2_item[eITEM_ADC37].status = eSTATUS_STOP;
    }

    // ADC40
    if (!m2_item[eITEM_ADC40].result) {
        m2_item[eITEM_ADC40].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_ADC40].ui_id, COLOR_YELLOW, -1);
        adc_value = adc_check (eADC_H40);
        memset  (str, 0, sizeof(str));  sprintf (str, "%d", adc_value);
        ui_set_sitem (p->pfb, p->pui, m2_item[eITEM_ADC40].ui_id, -1, -1, str);
        ui_set_ritem (p->pfb, p->pui, m2_item[eITEM_ADC40].ui_id, adc_value ? COLOR_GREEN : COLOR_RED, -1);
        m2_item[eITEM_ADC40].result = adc_value ? eRESULT_PASS : eRESULT_FAIL;
        m2_item[eITEM_ADC40].status = eSTATUS_STOP;
    }
    return 1;
}

//------------------------------------------------------------------------------
static int check_mac_addr (client_t *p)
{
    char str[32];

    efuse_set_board (eBOARD_ID_M2);

    m2_item[eITEM_MAC_ADDR].status = eSTATUS_RUN;
    ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_MAC_ADDR].ui_id, COLOR_YELLOW, -1);

    if (efuse_control (p->efuse_data, EFUSE_READ)) {
        efuse_get_mac (p->efuse_data, p->mac);
        if (!efuse_valid_check (p->efuse_data)) {
            if (mac_server_request (MAC_SERVER_FACTORY, REQ_TYPE_UUID, "m2",
                                    p->efuse_data)) {
                if (efuse_control (p->efuse_data, EFUSE_WRITE)) {
                    efuse_get_mac (p->efuse_data, p->mac);
                   if (efuse_valid_check (p->efuse_data))
                        m2_item [eITEM_MAC_ADDR].result = eRESULT_PASS;
                }
            }
        } else {
            m2_item [eITEM_MAC_ADDR].result = eRESULT_PASS;
        }
    }

    memset (str, 0, sizeof(str));
    sprintf(str, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
            p->mac[0],  p->mac[1], p->mac[2],
            p->mac[3],  p->mac[4], p->mac[5],
            p->mac[6],  p->mac[7], p->mac[8],
            p->mac[9], p->mac[10], p->mac[11]);

    ui_set_sitem (p->pfb, p->pui, m2_item [eITEM_MAC_ADDR].ui_id, -1, -1, str);
    m2_item[eITEM_MAC_ADDR].status = eSTATUS_STOP;

    if (m2_item [eITEM_MAC_ADDR].result) {
        ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_MAC_ADDR].ui_id, COLOR_GREEN, -1);
        tolowerstr (p->mac);
//        nlp_server_write (p->nlp_ip, NLP_SERVER_MSG_TYPE_MAC, p->mac, p->channel);
        return 1;
    }
    ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_MAC_ADDR].ui_id, COLOR_RED, -1);
    return 0;
}

//------------------------------------------------------------------------------
#define IPERF_SPEED_MIN 800

static int check_iperf_speed (client_t *p)
{
    int value = 0, retry = 3;
    char str[32];

retry_iperf:
    m2_item [eITEM_IPERF].status = eSTATUS_RUN;
    ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_IPERF].ui_id, COLOR_YELLOW, -1);
    nlp_server_write (p->nlp_ip, NLP_SERVER_MSG_TYPE_UDP, "start", 0);  usleep (APP_LOOP_DELAY * 1000);
    value = iperf3_speed_check(p->nlp_ip, NLP_SERVER_MSG_TYPE_UDP);
    nlp_server_write (p->nlp_ip, NLP_SERVER_MSG_TYPE_UDP, "stop", 0);   usleep (APP_LOOP_DELAY * 1000);

    memset  (str, 0, sizeof(str));
    sprintf (str, "%d Mbits/sec", value);

    ui_set_sitem (p->pfb, p->pui, m2_item [eITEM_IPERF].ui_id, -1, -1, str);
    ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_IPERF].ui_id, value > IPERF_SPEED_MIN ? COLOR_GREEN : COLOR_RED, -1);
    m2_item [eITEM_IPERF].result = value > IPERF_SPEED_MIN ? eRESULT_PASS : eRESULT_FAIL;
    m2_item [eITEM_IPERF].status = eSTATUS_STOP;

    if (!m2_item [eITEM_IPERF].result) {
        usleep (APP_LOOP_DELAY * 1000);
        if (retry) {    retry--;    goto retry_iperf;   }
    }
    return 1;
}

//------------------------------------------------------------------------------
#define I2C_ADC_DEV "gpio,sda,116,scl,117"

static int check_i2cadc (client_t *p)
{
    // ADC Board Check
    int value = 0, cnt = 1;

    if (p->adc_fd == 0 )    p->adc_fd = adc_board_init (I2C_ADC_DEV);

    if (p->adc_fd != 0 ) {
        // DC Jack 12V ~ 19V Check (2.4V ~ 3.8V)
        adc_board_read (p->adc_fd, "P13.2", &value, &cnt);
        if (value > 2000) {
            adc_board_read (p->adc_fd, "P3.2", &value, &cnt);
            p->channel = (value > 4000) ? NLP_SERVER_CHANNEL_RIGHT : NLP_SERVER_CHANNEL_LEFT;

            p->test_model = TEST_MODEL_NONE;
            // Test Model 8GB
            adc_board_read (p->adc_fd, "P3.8", &value, &cnt);
            if (value > 4000)
                p->test_model = TEST_MODEL_8GB;

            // Test Model 16GB
            adc_board_read (p->adc_fd, "P3.9", &value, &cnt);
            if (value > 4000)
                p->test_model = TEST_MODEL_16GB;

            return 1;
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
static int check_server (client_t *p)
{
    char ip_addr [IP_ADDR_SIZE];

    memset (ip_addr, 0, sizeof(ip_addr));

    m2_item [eITEM_BOARD_IP].status = m2_item [eITEM_SERVER_IP].status = eSTATUS_RUN;
    ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_BOARD_IP].ui_id, COLOR_YELLOW, -1);
    if (get_my_ip (ip_addr)) {
        ui_set_sitem (p->pfb, p->pui, m2_item [eITEM_BOARD_IP].ui_id, -1, -1, ip_addr);
        ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_BOARD_IP].ui_id, p->pui->bc.uint, -1);
        m2_item [eITEM_BOARD_IP].result = eRESULT_PASS;
        m2_item [eITEM_BOARD_IP].status = eSTATUS_STOP;

        memset (ip_addr, 0, sizeof(ip_addr));

        ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_SERVER_IP].ui_id, COLOR_YELLOW, -1);
        if (nlp_server_find(ip_addr)) {
            memcpy (p->nlp_ip, ip_addr, IP_ADDR_SIZE);
            ui_set_sitem (p->pfb, p->pui, m2_item [eITEM_SERVER_IP].ui_id, -1, -1, ip_addr);
            ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_SERVER_IP].ui_id, p->pui->bc.uint, -1);
            m2_item [eITEM_SERVER_IP].result = eRESULT_PASS;
            m2_item [eITEM_SERVER_IP].status = eSTATUS_STOP;
            return 1;
        } else {
            ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_SERVER_IP].ui_id, COLOR_RED, -1);
        }
    } else {
        ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_BOARD_IP].ui_id, COLOR_RED, -1);
    }

    return 0;
}

//------------------------------------------------------------------------------
static int audio_sine_wave (client_t *p, int ch)
{
    int value = 0, cnt = 0, loop, retry = 3;

    adc_board_read (p->adc_fd, ch ? "P13.3" : "P13.4", &value, &cnt);

    // default high
    if (value < 3000)   return 0;

    // Audio Busy check (0 : err, 1 : pass, 2 : busy)
    while (audio_check (ch) == 2)   usleep (100 * 1000);

    for (loop = 0; loop < retry; loop++) {
        adc_board_read (p->adc_fd, ch ? "P13.3" : "P13.4", &value, &cnt);
        if (value < 100)    return 1;
        usleep (100 * 1000);
    }
    return 0;
}

static int check_device_audio (client_t *p)
{
    if (!m2_item [eITEM_AUDIO_LEFT].result) {
        m2_item [eITEM_AUDIO_LEFT].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_AUDIO_LEFT].ui_id, COLOR_YELLOW, -1);

        if (audio_sine_wave (p, 0)) {
            m2_item [eITEM_AUDIO_LEFT].result = eRESULT_PASS;
            ui_set_sitem (p->pfb, p->pui, m2_item [eITEM_AUDIO_LEFT].ui_id, -1, -1, "PASS");
            ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_AUDIO_LEFT].ui_id, COLOR_GREEN, -1);
        } else {
            ui_set_sitem (p->pfb, p->pui, m2_item [eITEM_AUDIO_LEFT].ui_id, -1, -1, "FAIL");
            ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_AUDIO_LEFT].ui_id, COLOR_RED, -1);
        }
        m2_item [eITEM_AUDIO_LEFT].status = eSTATUS_STOP;
    }

    if (!m2_item [eITEM_AUDIO_RIGHT].result) {
        m2_item [eITEM_AUDIO_RIGHT].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_AUDIO_RIGHT].ui_id, COLOR_YELLOW, -1);

        if (audio_sine_wave (p, 1)) {
            m2_item [eITEM_AUDIO_RIGHT].result = eRESULT_PASS;
            ui_set_sitem (p->pfb, p->pui, m2_item [eITEM_AUDIO_RIGHT].ui_id, -1, -1, "PASS");
            ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_AUDIO_RIGHT].ui_id, COLOR_GREEN, -1);
        } else {
            ui_set_sitem (p->pfb, p->pui, m2_item [eITEM_AUDIO_RIGHT].ui_id, -1, -1, "FAIL");
            ui_set_ritem (p->pfb, p->pui, m2_item [eITEM_AUDIO_RIGHT].ui_id, COLOR_RED, -1);
        }
        m2_item [eITEM_AUDIO_RIGHT].status = eSTATUS_STOP;
    }
    return 1;
}

//------------------------------------------------------------------------------
static int client_setup (client_t *p)
{
    pthread_t thread_hp_detect, thread_check_status, thread_ethernet;
    pthread_t thread_usb, thread_storage;

    if ((p->pfb = fb_init (DEVICE_FB)) == NULL)         exit(1);
    if ((p->pui = ui_init (p->pfb, CONFIG_UI)) == NULL) exit(1);

    pthread_create (&thread_check_status, NULL, check_status, p);

    check_device_hdmi(p);   check_device_system (p);

    while (!check_server (p))   usleep (APP_LOOP_DELAY * 1000);

    pthread_create (&thread_hp_detect,    NULL, check_hp_detect, p);

    ethernet_link_setup (LINK_SPEED_1G);

    check_iperf_speed (p);
    check_mac_addr (p);

    pthread_create (&thread_usb,      NULL, check_device_usb, p);
    pthread_create (&thread_storage,  NULL, check_device_storage, p);
    pthread_create (&thread_ethernet, NULL, check_device_ethernet, p);

    return 1;
}

//------------------------------------------------------------------------------
int main (void)
{
    client_t client;
    pthread_t thread_sw_adc;

    memset (&client, 0, sizeof(client));

    // UI
    client_setup (&client);

    while (!check_i2cadc(&client))  usleep (APP_LOOP_DELAY * 1000);
    check_device_system (&client);

    pthread_create (&thread_sw_adc, NULL, check_sw_adc, &client);

    while (1)   {
        // retry
        check_device_hdmi   (&client);
        check_device_system (&client);
        check_device_adc    (&client);
        check_header        (&client);
        check_device_audio  (&client);
        usleep (APP_LOOP_DELAY * 1000);
    }

    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
