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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

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
#define TEST_MODEL_4GB  4
#define TEST_MODEL_8GB  8

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
    eITEM_MEM,
    eITEM_FB,

    // hdmi
    eITEM_EDID,
    eITEM_HPD,

    eITEM_STATUS,

    // storage
    eITEM_eMMC,
    eITEM_SATA,
    eITEM_NVME,

    eITEM_MAC_ADDR,
    eITEM_IPERF,

    // usb
    eITEM_ETHERNET_100M,
    eITEM_ETHERNET_1G,
    eITEM_ETHERNET_LED,

    eITEM_IR,

    eITEM_USB30_UP,
    eITEM_USB30_DN,
    eITEM_USB20_UP,
    eITEM_USB20_DN,

    eITEM_HEADER_PT1,
    eITEM_HEADER_PT2,
    eITEM_HEADER_PT3,
    eITEM_HEADER_PT4,

    eITEM_SPIBT_UP,
    eITEM_SPIBT_DN,

    // adc
    eITEM_ADC37,
    eITEM_ADC40,

    eITEM_AUDIO_LEFT,
    eITEM_AUDIO_RIGHT,

    // HP_DETECT
    eITEM_HPDET_IN,
    eITEM_HPDET_OUT,

    eITEM_END
};

enum {
    eUI_BOARD_IP = 4,
    eUI_SERVER_IP = 24,

    eUI_MEM = 8,
    eUI_FB = 52,

    eUI_EDID = 53,
    eUI_HPD = 54,

    eUI_STATUS = 47,

    eUI_eMMC = 62,
    eUI_SATA = 82,
    eUI_NVME = 87,

    eUI_MAC_ADDR = 102,
    eUI_IPERF = 107,

    eUI_ETHERNET_100M = 132,
    eUI_ETHERNET_1G = 133,
    eUI_ETHERNET_LED = 134,

    eUI_IR = 127,

    eUI_USB30_UP = 143,
    eUI_USB30_DN = 153,
    eUI_USB20_UP = 148,
    eUI_USB20_DN = 158,

    eUI_HEADER_PT1 = 172,
    eUI_HEADER_PT2 = 173,
    eUI_HEADER_PT3 = 174,
    eUI_HEADER_PT4 = 175,

    eUI_SPIBT_UP = 178,
    eUI_SPIBT_DN = 179,

    eUI_ADC37 = 192,
    eUI_ADC40 = 193,

    eUI_AUDIO_LEFT  = 196,
    eUI_AUDIO_RIGHT = 197,

    eUI_HPDET_IN  = 198,
    eUI_HPDET_OUT = 199,
    eUI_END
};

struct check_item {
    int id, ui_id, status, result;
    // item name for error
    const char *name;
};

struct check_item m1_item [eITEM_END] = {
    { eITEM_BOARD_IP,       eUI_BOARD_IP,       eSTATUS_WAIT, eRESULT_FAIL, "bip" },
    { eITEM_SERVER_IP,      eUI_SERVER_IP,      eSTATUS_WAIT, eRESULT_FAIL, "sip" },

    // system
    { eITEM_MEM,            eUI_MEM,            eSTATUS_WAIT, eRESULT_FAIL, "mem" },
    { eITEM_FB,             eUI_FB,             eSTATUS_WAIT, eRESULT_FAIL, "fb" },

    // hdmi
    { eITEM_EDID,           eUI_EDID,           eSTATUS_WAIT, eRESULT_FAIL, "edid" },
    { eITEM_HPD,            eUI_HPD,            eSTATUS_WAIT, eRESULT_FAIL, "hpd" },

    { eITEM_STATUS,         eUI_STATUS,         eSTATUS_STOP, eRESULT_PASS, "sta" },

    // storage
    { eITEM_eMMC,           eUI_eMMC,           eSTATUS_WAIT, eRESULT_FAIL, "emmc" },
    { eITEM_SATA,           eUI_SATA,           eSTATUS_WAIT, eRESULT_FAIL, "sata" },
    { eITEM_NVME,           eUI_NVME,           eSTATUS_WAIT, eRESULT_FAIL, "nvme" },

    { eITEM_MAC_ADDR,       eUI_MAC_ADDR,       eSTATUS_WAIT, eRESULT_FAIL, "mac" },
    { eITEM_IPERF,          eUI_IPERF,          eSTATUS_WAIT, eRESULT_FAIL, "iperf" },

    // usb
    { eITEM_ETHERNET_100M,  eUI_ETHERNET_100M,  eSTATUS_WAIT, eRESULT_FAIL, "eth-m" },
    { eITEM_ETHERNET_1G,    eUI_ETHERNET_1G,    eSTATUS_WAIT, eRESULT_FAIL, "eth-g" },
    { eITEM_ETHERNET_LED,   eUI_ETHERNET_LED,   eSTATUS_STOP, eRESULT_PASS, "eth-led" },

    { eITEM_IR,             eUI_IR,             eSTATUS_WAIT, eRESULT_FAIL, "ir" },

    { eITEM_USB30_UP,       eUI_USB30_UP,       eSTATUS_WAIT, eRESULT_FAIL, "usb3u" },
    { eITEM_USB30_DN,       eUI_USB30_DN,       eSTATUS_WAIT, eRESULT_FAIL, "usb3d" },
    { eITEM_USB20_UP,       eUI_USB20_UP,       eSTATUS_WAIT, eRESULT_FAIL, "usb2u" },
    { eITEM_USB20_DN,       eUI_USB20_DN,       eSTATUS_WAIT, eRESULT_FAIL, "usb2d" },

    { eITEM_HEADER_PT1,     eUI_HEADER_PT1,     eSTATUS_WAIT, eRESULT_FAIL, "h1" },
    { eITEM_HEADER_PT2,     eUI_HEADER_PT2,     eSTATUS_WAIT, eRESULT_FAIL, "h2" },
    { eITEM_HEADER_PT3,     eUI_HEADER_PT3,     eSTATUS_WAIT, eRESULT_FAIL, "h3" },
    { eITEM_HEADER_PT4,     eUI_HEADER_PT4,     eSTATUS_WAIT, eRESULT_FAIL, "h4" },

    { eITEM_SPIBT_UP,       eUI_SPIBT_UP,       eSTATUS_WAIT, eRESULT_FAIL, "bt-u" },
    { eITEM_SPIBT_DN,       eUI_SPIBT_DN,       eSTATUS_WAIT, eRESULT_FAIL, "bt_d" },

    // adc
    { eITEM_ADC37,          eUI_ADC37,          eSTATUS_WAIT, eRESULT_FAIL, "adc37" },
    { eITEM_ADC40,          eUI_ADC40,          eSTATUS_WAIT, eRESULT_FAIL, "adc40" },

    { eITEM_AUDIO_LEFT,     eUI_AUDIO_LEFT,     eSTATUS_WAIT, eRESULT_FAIL, "hp-l" },
    { eITEM_AUDIO_RIGHT,    eUI_AUDIO_RIGHT,    eSTATUS_WAIT, eRESULT_FAIL, "hp-r" },

    // HP_DETECT
    { eITEM_HPDET_IN,       eUI_HPDET_IN,       eSTATUS_WAIT, eRESULT_FAIL, "hp-i" },
    { eITEM_HPDET_OUT,      eUI_HPDET_OUT,      eSTATUS_WAIT, eRESULT_FAIL, "hp-o" },
};

//------------------------------------------------------------------------------
#define	RUN_BOX_ON	RGB_TO_UINT(204, 204, 0)
#define	RUN_BOX_OFF	RGB_TO_UINT(153, 153, 0)

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
        if (!m1_item[i].result) {
            if ((pos + strlen(m1_item[i].name) + 1) > PRINT_MAX_CHAR) {
                pos = 0, line++;
            }
            pos += sprintf (&err_msg[line][pos], "%s,", m1_item[i].name);
            ui_set_ritem (p->pfb, p->pui, m1_item [i].ui_id, COLOR_RED, -1);
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
enum {
    eEVENT_NONE,
    eEVENT_ETH_GLED,
    eEVENT_ETH_OLED,
    eEVENT_HP_L,
    eEVENT_HP_R,
    eEVENT_MAC_PRINT,
    eEVENT_STOP,
    eEVENT_ENTER,
    eEVENT_BACK,
    eEVENT_END
};

static int EventIR = eEVENT_NONE;

void *check_device_ir (void *arg);
void *check_device_ir (void *arg)
{
    client_t *p = (client_t *)arg;

    struct input_event event;
    struct timeval  timeout;
    fd_set readFds;
    static int fd = -1;

    // IR Device Name
    // /sys/class/input/event0/device/name -> fdd70030.pwm
    if ((fd = open("/dev/input/event0", O_RDONLY)) < 0) {
        printf ("%s : /dev/input/event0 open error!\n", __func__);
        return arg;
    }
    printf("%s fd = %d\n", __func__, fd);

    m1_item[eITEM_IR].status = eSTATUS_RUN;
    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_IR].ui_id, RUN_BOX_ON, -1);

    while (1) {
        // recive time out config
        // Set 1ms timeout counter
        timeout.tv_sec  = 0;
        // timeout.tv_usec = timeout_ms*1000;
        timeout.tv_usec = 100000;

        FD_ZERO(&readFds);
        FD_SET(fd, &readFds);
        select(fd+1, &readFds, NULL, NULL, &timeout);

        if(FD_ISSET(fd, &readFds))
        {
            if(fd && read(fd, &event, sizeof(struct input_event))) {

                switch (event.type) {
                    case    EV_SYN:
                        break;
                    case    EV_KEY:
                        ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_IR].ui_id, -1, -1, "PASS");
                        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_IR].ui_id, COLOR_GREEN, -1);
                        m1_item[eITEM_IR].result = eRESULT_PASS;
                        m1_item[eITEM_IR].status = eSTATUS_STOP;

                        switch (event.code) {
                            /* emergency stop */
                            case    KEY_HOME:
                                printf ("%s : EmergencyStop!!\n", __func__);
                                EventIR = eEVENT_STOP;
                                break;
                            case    KEY_VOLUMEDOWN:
                                EventIR = eEVENT_ETH_GLED;
                                break;
                            case    KEY_VOLUMEUP:
                                EventIR = eEVENT_ETH_OLED;
                                break;
                            case    KEY_MENU:
                                EventIR = eEVENT_MAC_PRINT;
                                break;
                            case    KEY_LEFT:
                                EventIR = eEVENT_HP_L;
                                break;
                            case    KEY_RIGHT:
                                EventIR = eEVENT_HP_R;
                                break;
                            case    KEY_ENTER:
                                EventIR = eEVENT_ENTER;
                                break;
                            case    KEY_BACK:
                                EventIR = eEVENT_BACK;
                                break;
                            default :
                                EventIR = eEVENT_NONE;
                                break;
                        }
                        break;
                    default :
                        printf("unknown event\n");
                        break;
                }
            }
        }
    }
    return arg;
}

//------------------------------------------------------------------------------
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

        if (m1_item[eITEM_SERVER_IP].result && TimeoutStop) {
            memset (str, 0, sizeof(str));
            if (p->adc_fd != -1) {
                ui_set_ritem (p->pfb, p->pui, eUI_STATUS, onoff ? RUN_BOX_ON : RUN_BOX_OFF, -1);
                sprintf (str, "RUNNING %d", TimeoutStop);
            } else {
                ui_set_ritem (p->pfb, p->pui, eUI_STATUS, onoff ? COLOR_RED : p->pui->bc.uint, -1);
                sprintf (str, "I2CADC %d", TimeoutStop);
            }
            ui_set_sitem (p->pfb, p->pui, eUI_STATUS, -1, -1, str);
        }
        if (onoff) {
            ui_update (p->pfb, p->pui, -1);
            if (TimeoutStop && (p->adc_fd != -1))   TimeoutStop--;
        }

        led_set_status (eLED_POWER, onoff);
        led_set_status (eLED_ALIVE, onoff);
        usleep (APP_LOOP_DELAY * 1000);
        {
            int wait_item_cnt = 0, i;

            for (i = 0; i < eITEM_END; i++) {
                if (m1_item[i].status != eSTATUS_STOP) wait_item_cnt++;
            }
            if (!wait_item_cnt) {
                TimeoutStop = 0;    break;
            }
        }
    }

    {
        int stop_cnt = 0, i;

        for (i = 0; i < eITEM_END; i++) {
            if (m1_item[i].status == eSTATUS_STOP) stop_cnt++;
            else
                printf ("not STOP = %s\n", m1_item[i].name);
        }
        printf ("stop_cnt = %d,%d\n", eITEM_END, stop_cnt);
    }
    // ethernet switch thread check.
    while (p->eth_switch)   usleep (APP_LOOP_DELAY * 1000);

    // display stop
    memset (str, 0, sizeof(str));   sprintf (str, "%s", "FINISH");
    ethernet_link_setup (LINK_SPEED_1G);
    // wait for network stable
    usleep (APP_LOOP_DELAY * 1000);

    if (m1_item [eITEM_MAC_ADDR].result)
        nlp_server_write (p->nlp_ip, NLP_SERVER_MSG_TYPE_MAC, p->mac, p->channel);
    ui_set_sitem (p->pfb, p->pui, eUI_STATUS, -1, -1, str);
    err = errcode_print (p);
    ui_set_ritem (p->pfb, p->pui, eUI_STATUS, err ? COLOR_RED : COLOR_GREEN, -1);

    while (1) {
        usleep (APP_LOOP_DELAY * 1000);
        onoff = !onoff;

        led_set_status (eLED_POWER, onoff);
        led_set_status (eLED_ALIVE, onoff);

        if (onoff)
            ui_set_ritem (p->pfb, p->pui, eUI_STATUS, err ? COLOR_RED : COLOR_GREEN, -1);
        else
            ui_set_ritem (p->pfb, p->pui, eUI_STATUS, p->pui->bc.uint, -1);
        ui_update    (p->pfb, p->pui, -1);
    }
    return arg;
}

//------------------------------------------------------------------------------
static int JackStatus = 0;

void *check_hp_detect (void *arg);
void *check_hp_detect (void *arg)
{
    client_t *p = (client_t *)arg;
    struct input_event event;
    struct timeval  timeout;
    fd_set readFds;
    int fd;

    m1_item[eITEM_HPDET_IN].status = m1_item[eITEM_HPDET_OUT].status = eSTATUS_RUN;
    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_HPDET_IN].ui_id,  RUN_BOX_ON, -1);
    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_HPDET_OUT].ui_id, RUN_BOX_ON, -1);

    if ((fd = open("/dev/input/event2", O_RDONLY)) < 0) {
        printf ("%s : /dev/input/event2 open error!\n", __func__);
        return arg;
    }

    while (1) {
        // recive time out config
        // Set 1ms timeout counter
        timeout.tv_sec  = 0;
        // timeout.tv_usec = timeout_ms*1000;
        timeout.tv_usec = 100000;

        FD_ZERO(&readFds);
        FD_SET(fd, &readFds);
        select(fd+1, &readFds, NULL, NULL, &timeout);

        if(FD_ISSET(fd, &readFds)) {
            if(fd && read(fd, &event, sizeof(struct input_event))) {
                switch (event.type) {
                    case    EV_SYN:
                        break;
                    case    EV_SW:
                        switch (event.code) {
                            case    SW_HEADPHONE_INSERT:
                                if (event.value) {
                                    ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_HPDET_IN].ui_id, -1, -1, "PASS");
                                    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_HPDET_IN].ui_id, COLOR_GREEN, -1);
                                    m1_item[eITEM_HPDET_IN].status = eSTATUS_STOP;
                                    m1_item[eITEM_HPDET_IN].result = eRESULT_PASS;
                                    JackStatus = 1;
                                } else {
                                    ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_HPDET_OUT].ui_id, -1, -1, "PASS");
                                    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_HPDET_OUT].ui_id, COLOR_GREEN, -1);
                                    m1_item[eITEM_HPDET_OUT].status = eSTATUS_STOP;
                                    m1_item[eITEM_HPDET_OUT].result = eRESULT_PASS;
                                    JackStatus = 0;
                                }
                                break;
                            default :
                                break;
                        }
                        break;
                    default :
                        break;
                }
            }
        }
    }
    return arg;
}

//------------------------------------------------------------------------------
const char *EFUSE_UUID_FILE = "/sys/class/efuse/uuid";

int get_efuse_mac (char *mac_str)
{
    FILE *fp;
    char cmd_line[128];

    if (access (EFUSE_UUID_FILE, F_OK) != 0)
        return 0;

    if ((fp = fopen(EFUSE_UUID_FILE, "r")) != NULL) {
        if (fgets (cmd_line, sizeof(cmd_line), fp) != NULL) {
            char *ptr;
            int i;
            tolowerstr (cmd_line);
            if ((ptr = strstr(cmd_line, "001e06")) != NULL) {
                for (i = 0; i < 12; i++)
                    mac_str[i] = *(ptr + i);
                printf ("%s : mac str = %s\n", __func__, mac_str);
                fclose (fp);
                return 1;
            } else {
                /* display hex value */
                int i, len = strlen(cmd_line);
                for (i = 0; i < len; i++)
                    printf ("%d - 0x%02x\n", i, cmd_line[i]);
            }
        }
        fclose(fp);
    }
    return 0;
}

void *check_spibt (void *arg);
void *check_spibt (void *arg)
{
    client_t *p = (client_t *)arg;
    char mac_str[20], status = get_efuse_mac(mac_str);

    m1_item[eITEM_SPIBT_UP].status = eSTATUS_RUN;
    m1_item[eITEM_SPIBT_DN].status = eSTATUS_RUN;
    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_SPIBT_UP].ui_id, RUN_BOX_ON, -1);
    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_SPIBT_DN].ui_id, RUN_BOX_ON, -1);
    while (1) {
        if (m1_item[eITEM_SPIBT_UP].result != eRESULT_PASS) {
            if (status != get_efuse_mac(mac_str)) {
                status = get_efuse_mac(mac_str);
                ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_SPIBT_UP].ui_id, -1, -1, "PASS");
                ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_SPIBT_UP].ui_id, COLOR_GREEN, -1);
                m1_item[eITEM_SPIBT_UP].result = eRESULT_PASS;
                m1_item[eITEM_SPIBT_UP].status = eSTATUS_STOP;
            }
        }
        if (m1_item[eITEM_SPIBT_DN].result != eRESULT_PASS) {
            if (status != get_efuse_mac(mac_str)) {
                status = get_efuse_mac(mac_str);
                ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_SPIBT_DN].ui_id, -1, -1, "PASS");
                ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_SPIBT_DN].ui_id, COLOR_GREEN, -1);
                m1_item[eITEM_SPIBT_DN].result = eRESULT_PASS;
                m1_item[eITEM_SPIBT_DN].status = eSTATUS_STOP;
            }
        }
        usleep (APP_LOOP_DELAY * 1000);
    }
    return arg;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int check_device_ethernet (client_t *p)
{
    int speed;

    speed = ethernet_link_check ();

    if ((EventIR == eEVENT_ETH_GLED) && (speed != LINK_SPEED_100M)) {

        m1_item[eITEM_ETHERNET_100M].status = eSTATUS_RUN;

        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_100M].ui_id, COLOR_YELLOW, -1);
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_LED].ui_id, COLOR_YELLOW, -1);
        if (ethernet_link_setup (LINK_SPEED_100M)) {
            m1_item[eITEM_ETHERNET_100M].status = eSTATUS_STOP;
            m1_item[eITEM_ETHERNET_100M].result = eRESULT_PASS;
            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_100M].ui_id, -1, -1, "PASS");
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_100M].ui_id, COLOR_GREEN, -1);

            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_LED].ui_id, -1, -1, "GREEN");
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_LED].ui_id, COLOR_DARK_CYAN, -1);
            return 1;
        }
    }

    if ((EventIR == eEVENT_ETH_OLED) && (speed != LINK_SPEED_1G)) {

        m1_item[eITEM_ETHERNET_1G].status = eSTATUS_RUN;

        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_1G].ui_id, COLOR_YELLOW, -1);
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_LED].ui_id, COLOR_YELLOW, -1);
        if (ethernet_link_setup (LINK_SPEED_1G)) {
            m1_item[eITEM_ETHERNET_1G].status = eSTATUS_STOP;
            m1_item[eITEM_ETHERNET_1G].result = eRESULT_PASS;
            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_1G].ui_id, -1, -1, "PASS");
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_1G].ui_id, COLOR_GREEN, -1);

            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_LED].ui_id, -1, -1, "ORANGE");
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ETHERNET_LED].ui_id, COLOR_DARK_KHAKI, -1);
            return 1;
        }
    }
    return 0;
}

//------------------------------------------------------------------------------
void *check_device_usb (void *arg);
void *check_device_usb (void *arg)
{
    int value = 0;
    char str[10];
    client_t *p = (client_t *)arg;

    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB30_UP].ui_id, RUN_BOX_ON, -1);
    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB30_DN].ui_id, RUN_BOX_ON, -1);
    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB20_UP].ui_id, RUN_BOX_ON, -1);
    ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB20_DN].ui_id, RUN_BOX_ON, -1);

    while (1) {
        // USB30
        if (!m1_item[eITEM_USB30_UP].result && usb_check (eUSB30_UP_R)) {
            m1_item[eITEM_USB30_UP].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB30_UP].ui_id, COLOR_YELLOW, -1);
            value = usb_rw (eUSB30_UP_R);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);
            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_USB30_UP].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB30_UP].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m1_item[eITEM_USB30_UP].result = value ? eRESULT_PASS : eRESULT_FAIL;
            m1_item[eITEM_USB30_UP].status = eSTATUS_STOP;
        }
        if (!m1_item[eITEM_USB30_DN].result && usb_check (eUSB30_DN_R)) {
            m1_item[eITEM_USB30_DN].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB30_DN].ui_id, COLOR_YELLOW, -1);
            value = usb_rw (eUSB30_DN_R);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);
            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_USB30_DN].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB30_DN].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m1_item[eITEM_USB30_DN].result = value ? eRESULT_PASS : eRESULT_FAIL;
            m1_item[eITEM_USB30_DN].status = eSTATUS_STOP;
        }

        // USB20
        if (!m1_item[eITEM_USB20_UP].result && usb_check (eUSB20_UP_R)) {
            m1_item[eITEM_USB20_UP].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB20_UP].ui_id, COLOR_YELLOW, -1);
            value = usb_rw (eUSB20_UP_R);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);
            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_USB20_UP].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB20_UP].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m1_item[eITEM_USB20_UP].result = value ? eRESULT_PASS : eRESULT_FAIL;
            m1_item[eITEM_USB20_UP].status = eSTATUS_STOP;
        }
        if (!m1_item[eITEM_USB20_DN].result && usb_check (eUSB20_DN_R)) {
            m1_item[eITEM_USB20_DN].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB20_DN].ui_id, COLOR_YELLOW, -1);
            value = usb_rw (eUSB20_DN_R);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);
            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_USB20_DN].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_USB20_DN].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m1_item[eITEM_USB20_DN].result = value ? eRESULT_PASS : eRESULT_FAIL;
            m1_item[eITEM_USB20_DN].status = eSTATUS_STOP;
        }

        if (m1_item[eITEM_USB30_UP].result && m1_item[eITEM_USB30_DN].result &&
            m1_item[eITEM_USB20_UP].result && m1_item[eITEM_USB20_DN].result)
            break;

        usleep (APP_LOOP_DELAY * 1000);
    }

    return arg;
}

//------------------------------------------------------------------------------
static int check_header (client_t *p)
{
    static int init = 0;
    int ui_id = m1_item[eITEM_HEADER_PT1].ui_id, i;
    int pattern40[40 +1], cnt;

    if (!init)  {   header_init (); init = 1; }

    for (i = 0; i < eHEADER_END; i++) {
        if (!m1_item[eITEM_HEADER_PT1 + i].result) {
            m1_item[eITEM_HEADER_PT1 + i].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, ui_id + i, COLOR_YELLOW, -1);

            header_pattern_set (i); usleep (100 * 1000);

            memset (pattern40, 0, sizeof(pattern40));
            adc_board_read (p->adc_fd,  "CON1", &pattern40[1],  &cnt);
            if (header_pattern_check (i, pattern40)) {
                m1_item[eITEM_HEADER_PT1 + i].result = eRESULT_PASS;
                ui_set_sitem (p->pfb, p->pui, ui_id + i, -1, -1, "PASS");
                ui_set_ritem (p->pfb, p->pui, ui_id + i, COLOR_GREEN, -1);
            } else {
                m1_item[eITEM_HEADER_PT1 + i].result = eRESULT_FAIL;
                ui_set_sitem (p->pfb, p->pui, ui_id + i, -1, -1, "FAIL");
                ui_set_ritem (p->pfb, p->pui, ui_id + i, COLOR_RED, -1);
            }
            m1_item[eITEM_HEADER_PT1 + i].status = eSTATUS_STOP;
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
        if (!m1_item [eITEM_eMMC].result && storage_check (eSTORAGE_eMMC)) {
            m1_item[eITEM_eMMC].status = eSTATUS_RUN;

            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_eMMC].ui_id, COLOR_YELLOW, -1);
            value = storage_rw (eSTORAGE_eMMC);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);

            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_eMMC].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_eMMC].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m1_item[eITEM_eMMC].result = value ? eRESULT_PASS : eRESULT_FAIL;

            if (m1_item[eITEM_eMMC].result) m1_item[eITEM_eMMC].status = eSTATUS_STOP;
        }

        // SATA
        if (!m1_item [eITEM_SATA].result && storage_check (eSTORAGE_SATA)) {
            m1_item[eITEM_SATA].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_SATA].ui_id, COLOR_YELLOW, -1);
            value = storage_rw (eSTORAGE_SATA);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);

            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_SATA].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_SATA].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m1_item[eITEM_SATA].result = value ? eRESULT_PASS : eRESULT_FAIL;

            if (m1_item[eITEM_SATA].result)  m1_item[eITEM_SATA].status = eSTATUS_STOP;
        }

        // NVME
        if (!m1_item [eITEM_NVME].result && storage_check (eSTORAGE_NVME)) {
            m1_item[eITEM_NVME].status = eSTATUS_RUN;
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_NVME].ui_id, COLOR_YELLOW, -1);
            value = storage_rw (eSTORAGE_NVME);
            memset (str, 0, sizeof(str));   sprintf(str, "%d MB/s", value);

            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_NVME].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_NVME].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m1_item[eITEM_NVME].result = value ? eRESULT_PASS : eRESULT_FAIL;

            if (m1_item[eITEM_NVME].result) m1_item[eITEM_NVME].status = eSTATUS_STOP;
        }
        if (m1_item [eITEM_eMMC].result && m1_item [eITEM_SATA].result && m1_item [eITEM_NVME].result)
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
    if (TimeoutStop) {
        m1_item[eITEM_MEM].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_MEM].ui_id, COLOR_YELLOW, -1);
        value = system_check (eSYSTEM_MEM);
        p->board_mem = value;
        memset (str, 0, sizeof(str));
        if (p->test_model) {
            sprintf (str, "%d / T-%d GB", p->board_mem, p->test_model);
            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_MEM].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_MEM].ui_id,
                            (p->test_model == p->board_mem) ? COLOR_GREEN : COLOR_RED, -1);
        } else {
            sprintf(str, "%d GB", value);
            ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_MEM].ui_id, -1, -1, str);
            ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_MEM].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
            m1_item[eITEM_MEM].result = value ? eRESULT_PASS : eRESULT_FAIL;
        }
        m1_item[eITEM_MEM].status = eSTATUS_STOP;
    }

    // FB
    if (!m1_item[eITEM_FB].result) {
        m1_item[eITEM_FB].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_FB].ui_id, COLOR_YELLOW, -1);
        value = system_check (eSYSTEM_FB_Y);
        memset (str, 0, sizeof(str));   sprintf(str, "%dP", value);

        ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_FB].ui_id, -1, -1, str);
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_FB].ui_id, (value == 1080) ? COLOR_GREEN : COLOR_RED, -1);
        m1_item[eITEM_FB].result = (value == 1080) ? eRESULT_PASS : eRESULT_FAIL;
        m1_item[eITEM_FB].status = eSTATUS_STOP;
    }

    if (p->test_model && (p->test_model != p->board_mem))
        m1_item[eITEM_MEM].result = eRESULT_FAIL;

    return 1;
}

//------------------------------------------------------------------------------
static int check_device_hdmi (client_t *p)
{
    int value = 0;

    // EDID
    if (!m1_item[eITEM_EDID].result) {
        m1_item[eITEM_EDID].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_EDID].ui_id, COLOR_YELLOW, -1);
        value = hdmi_check (eHDMI_EDID);
        ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_EDID].ui_id, -1, -1, value ? "PASS":"FAIL");
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_EDID].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
        m1_item[eITEM_EDID].result = value ? eRESULT_PASS : eRESULT_FAIL;
        m1_item[eITEM_EDID].status = eSTATUS_STOP;
    }

    // HPD
    if (!m1_item[eITEM_HPD].result) {
        m1_item[eITEM_HPD].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_HPD].ui_id, COLOR_YELLOW, -1);
        value = hdmi_check (eHDMI_HPD);
        ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_HPD].ui_id, -1, -1, value ? "PASS":"FAIL");
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_HPD].ui_id, value ? COLOR_GREEN : COLOR_RED, -1);
        m1_item[eITEM_HPD].result = value ? eRESULT_PASS : eRESULT_FAIL;
        m1_item[eITEM_HPD].status = eSTATUS_STOP;
    }

    return 1;
}

//------------------------------------------------------------------------------
static int check_device_adc (client_t *p)
{
    int adc_value = 0;
    char str[10];

    // ADC37
    if (!m1_item[eITEM_ADC37].result) {
        m1_item[eITEM_ADC37].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ADC37].ui_id, COLOR_YELLOW, -1);
        adc_value = adc_check (eADC_H37);
        memset  (str, 0, sizeof(str));  sprintf (str, "%d", adc_value);
        ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_ADC37].ui_id, -1, -1, str);
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ADC37].ui_id, adc_value ? COLOR_GREEN : COLOR_RED, -1);
        m1_item[eITEM_ADC37].result = adc_value ? eRESULT_PASS : eRESULT_FAIL;
        m1_item[eITEM_ADC37].status = eSTATUS_STOP;
    }

    // ADC40
    if (!m1_item[eITEM_ADC40].result) {
        m1_item[eITEM_ADC40].status = eSTATUS_RUN;
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ADC40].ui_id, COLOR_YELLOW, -1);
        adc_value = adc_check (eADC_H40);
        memset  (str, 0, sizeof(str));  sprintf (str, "%d", adc_value);
        ui_set_sitem (p->pfb, p->pui, m1_item[eITEM_ADC40].ui_id, -1, -1, str);
        ui_set_ritem (p->pfb, p->pui, m1_item[eITEM_ADC40].ui_id, adc_value ? COLOR_GREEN : COLOR_RED, -1);
        m1_item[eITEM_ADC40].result = adc_value ? eRESULT_PASS : eRESULT_FAIL;
        m1_item[eITEM_ADC40].status = eSTATUS_STOP;
    }
    return 1;
}

//------------------------------------------------------------------------------
static int check_mac_addr (client_t *p)
{
    char str[32];

    efuse_set_board (eBOARD_ID_M1);

    m1_item[eITEM_MAC_ADDR].status = eSTATUS_RUN;
    ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_MAC_ADDR].ui_id, COLOR_YELLOW, -1);

    if (efuse_control (p->efuse_data, EFUSE_READ)) {
        efuse_get_mac (p->efuse_data, p->mac);
        if (!efuse_valid_check (p->efuse_data)) {
            if (mac_server_request (MAC_SERVER_FACTORY, REQ_TYPE_UUID, "m1",
                                    p->efuse_data)) {
                if (efuse_control (p->efuse_data, EFUSE_WRITE)) {
                    efuse_get_mac (p->efuse_data, p->mac);
                   if (efuse_valid_check (p->efuse_data))
                        m1_item [eITEM_MAC_ADDR].result = eRESULT_PASS;
                }
            }
        } else {
            m1_item [eITEM_MAC_ADDR].result = eRESULT_PASS;
        }
    }

    memset (str, 0, sizeof(str));
    sprintf(str, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
            p->mac[0],  p->mac[1], p->mac[2],
            p->mac[3],  p->mac[4], p->mac[5],
            p->mac[6],  p->mac[7], p->mac[8],
            p->mac[9], p->mac[10], p->mac[11]);

    ui_set_sitem (p->pfb, p->pui, m1_item [eITEM_MAC_ADDR].ui_id, -1, -1, str);
    m1_item[eITEM_MAC_ADDR].status = eSTATUS_STOP;

    if (m1_item [eITEM_MAC_ADDR].result) {
        ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_MAC_ADDR].ui_id, COLOR_GREEN, -1);
        tolowerstr (p->mac);
//        nlp_server_write (p->nlp_ip, NLP_SERVER_MSG_TYPE_MAC, p->mac, p->channel);
        return 1;
    }
    ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_MAC_ADDR].ui_id, COLOR_RED, -1);
    return 0;
}

//------------------------------------------------------------------------------
#define IPERF_SPEED_MIN 800

static int check_iperf_speed (client_t *p)
{
    int value = 0, retry = 3;
    char str[32];

retry_iperf:
    m1_item [eITEM_IPERF].status = eSTATUS_RUN;
    ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_IPERF].ui_id, COLOR_YELLOW, -1);
    nlp_server_write (p->nlp_ip, NLP_SERVER_MSG_TYPE_UDP, "start", 0);  usleep (APP_LOOP_DELAY * 1000);
    value = iperf3_speed_check(p->nlp_ip, NLP_SERVER_MSG_TYPE_UDP);
    nlp_server_write (p->nlp_ip, NLP_SERVER_MSG_TYPE_UDP, "stop", 0);   usleep (APP_LOOP_DELAY * 1000);

    memset  (str, 0, sizeof(str));
    sprintf (str, "%d Mbits/sec", value);

    ui_set_sitem (p->pfb, p->pui, m1_item [eITEM_IPERF].ui_id, -1, -1, str);
    ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_IPERF].ui_id, value > IPERF_SPEED_MIN ? COLOR_GREEN : COLOR_RED, -1);
    m1_item [eITEM_IPERF].result = value > IPERF_SPEED_MIN ? eRESULT_PASS : eRESULT_FAIL;
    m1_item [eITEM_IPERF].status = eSTATUS_STOP;

    if (!m1_item [eITEM_IPERF].result) {
        usleep (APP_LOOP_DELAY * 1000);
        if (retry) {    retry--;    goto retry_iperf;   }
    }
    return 1;
}

//------------------------------------------------------------------------------
#define I2C_ADC_DEV "gpio,scl,109,sda,110"

static int check_i2cadc (client_t *p)
{
    // ADC Board Check
    int value = 0, cnt = 1;

    p->adc_fd = adc_board_init (I2C_ADC_DEV);

    if (p->adc_fd == 0 || p->adc_fd == -1)  return 0;

    // DC Jack 12V ~ 19V Check (2.4V ~ 3.8V)
    adc_board_read (p->adc_fd, "P13.2", &value, &cnt);
    if (value > 2000) {
        adc_board_read (p->adc_fd, "P3.2", &value, &cnt);
        p->channel = (value > 4000) ? NLP_SERVER_CHANNEL_RIGHT : NLP_SERVER_CHANNEL_LEFT;

        p->test_model = TEST_MODEL_NONE;
        // Test Model 8GB
        adc_board_read (p->adc_fd, "P3.8", &value, &cnt);
        if (value > 4000)
            p->test_model = TEST_MODEL_4GB;

        // Test Model 16GB
        adc_board_read (p->adc_fd, "P3.9", &value, &cnt);
        if (value > 4000)
            p->test_model = TEST_MODEL_8GB;

        return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------
static int check_server (client_t *p)
{
    char ip_addr [IP_ADDR_SIZE];

    memset (ip_addr, 0, sizeof(ip_addr));

    m1_item [eITEM_BOARD_IP].status = m1_item [eITEM_SERVER_IP].status = eSTATUS_RUN;
    ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_BOARD_IP].ui_id, COLOR_YELLOW, -1);
    if (get_my_ip (ip_addr)) {
        ui_set_sitem (p->pfb, p->pui, m1_item [eITEM_BOARD_IP].ui_id, -1, -1, ip_addr);
        ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_BOARD_IP].ui_id, p->pui->bc.uint, -1);
        m1_item [eITEM_BOARD_IP].result = eRESULT_PASS;
        m1_item [eITEM_BOARD_IP].status = eSTATUS_STOP;

        memset (ip_addr, 0, sizeof(ip_addr));

        ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_SERVER_IP].ui_id, COLOR_YELLOW, -1);
        if (nlp_server_find(ip_addr)) {
            memcpy (p->nlp_ip, ip_addr, IP_ADDR_SIZE);
            ui_set_sitem (p->pfb, p->pui, m1_item [eITEM_SERVER_IP].ui_id, -1, -1, ip_addr);
            ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_SERVER_IP].ui_id, p->pui->bc.uint, -1);
            m1_item [eITEM_SERVER_IP].result = eRESULT_PASS;
            m1_item [eITEM_SERVER_IP].status = eSTATUS_STOP;
            return 1;
        } else {
            ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_SERVER_IP].ui_id, COLOR_RED, -1);
        }
    } else {
        ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_BOARD_IP].ui_id, COLOR_RED, -1);
    }

    return 0;
}

//------------------------------------------------------------------------------
static int check_device_audio (client_t *p)
{
    if (!JackStatus)    return 0;

    if (EventIR == eEVENT_HP_L) {
        if (audio_check (eAUDIO_LEFT)) {
            m1_item [eITEM_AUDIO_LEFT].result = eRESULT_PASS;
            ui_set_sitem (p->pfb, p->pui, m1_item [eITEM_AUDIO_LEFT].ui_id, -1, -1, "PASS");
            ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_AUDIO_LEFT].ui_id, COLOR_GREEN, -1);
            m1_item [eITEM_AUDIO_LEFT].status = eSTATUS_STOP;
        }
        else return 0;
    }
    if (EventIR == eEVENT_HP_R) {
        if (audio_check (eAUDIO_RIGHT)) {
            m1_item [eITEM_AUDIO_RIGHT].result = eRESULT_PASS;
            ui_set_sitem (p->pfb, p->pui, m1_item [eITEM_AUDIO_RIGHT].ui_id, -1, -1, "PASS");
            ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_AUDIO_RIGHT].ui_id, COLOR_GREEN, -1);
            m1_item [eITEM_AUDIO_RIGHT].status = eSTATUS_STOP;
        }
        else return 0;
    }
    return 1;
}

//------------------------------------------------------------------------------
static int client_setup (client_t *p)
{
    pthread_t thread_hp_detect, thread_check_status;
    pthread_t thread_usb, thread_storage, thread_ir;

    if ((p->pfb = fb_init (DEVICE_FB)) == NULL)         exit(1);
    if ((p->pui = ui_init (p->pfb, CONFIG_UI)) == NULL) exit(1);

    pthread_create (&thread_check_status, NULL, check_status, p);

    check_device_hdmi(p);   check_device_system (p);

    while (!check_server (p))   usleep (APP_LOOP_DELAY * 1000);

    ethernet_link_setup (LINK_SPEED_1G);

    ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_ETHERNET_1G].ui_id,   RUN_BOX_ON, -1);
    ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_ETHERNET_100M].ui_id, RUN_BOX_ON, -1);
    ui_set_sitem (p->pfb, p->pui, m1_item [eITEM_ETHERNET_LED].ui_id, -1, -1, "Orange");

    ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_AUDIO_LEFT].ui_id,  RUN_BOX_ON, -1);
    ui_set_ritem (p->pfb, p->pui, m1_item [eITEM_AUDIO_RIGHT].ui_id, RUN_BOX_ON, -1);

    check_mac_addr (p);
    check_iperf_speed (p);

    pthread_create (&thread_storage,    NULL, check_device_storage, p);
    pthread_create (&thread_hp_detect,  NULL, check_hp_detect, p);
    pthread_create (&thread_ir,         NULL, check_device_ir, p);
    pthread_create (&thread_usb,        NULL, check_device_usb, p);

    return 1;
}

//------------------------------------------------------------------------------
int main (void)
{
    client_t client;
    pthread_t thread_spibt;

    memset (&client, 0, sizeof(client));

    // UI
    client_setup (&client);

    while (!check_i2cadc(&client))  sleep (1);
    check_device_system (&client);

    pthread_create (&thread_spibt, NULL, check_spibt, &client);

    while (1)   {
        // retry
        check_device_hdmi   (&client);
        check_device_system (&client);
        check_device_adc    (&client);
        check_header        (&client);
        usleep (APP_LOOP_DELAY * 1000);

        if (EventIR != eEVENT_NONE) {
            switch (EventIR) {
                case eEVENT_ETH_GLED:
                case eEVENT_ETH_OLED:
                    check_device_ethernet (&client);
                    break;
                case eEVENT_HP_L:
                case eEVENT_HP_R:
                    check_device_audio (&client);
                    break;
                case eEVENT_MAC_PRINT:
                    if (m1_item [eITEM_MAC_ADDR].result)
                        nlp_server_write (client.nlp_ip, NLP_SERVER_MSG_TYPE_MAC, client.mac, client.channel);
                    break;
                case eEVENT_STOP:
                    TimeoutStop = 0;
                    break;
                case eEVENT_ENTER:
                    if (!m1_item [eITEM_IPERF].result)
                        check_iperf_speed (&client);
                    break;
                case eEVENT_BACK:
                    printf ("Program restart!!\n"); fflush(stdout);

                    fb_clear  (client.pfb);
                    draw_text (client.pfb, 1920/4, 1080/2, COLOR_RED, COLOR_BLACK, 5, "- APPLICATION RESTART -");
                    return 0;
                default :
                    break;
            }
            EventIR = eEVENT_NONE;
        }
    }

    return 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
