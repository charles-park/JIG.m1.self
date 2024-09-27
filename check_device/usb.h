//------------------------------------------------------------------------------
/**
 * @file usb.h
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
#ifndef __USB_H__
#define __USB_H__

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Define the Device ID for the USB group.
//------------------------------------------------------------------------------
// ODROID-M1 USB Port define
enum {
    // USB 3.0
    eUSB30_UP_R,
    eUSB30_DN_R,
    // USB 2.0
    eUSB20_UP_R,
    eUSB20_DN_R,

    // USB 3.0
    eUSB30_UP_W,
    eUSB30_DN_W,
    // USB 2.0
    eUSB20_UP_W,
    eUSB20_DN_W,

    eUSB_END
};

//------------------------------------------------------------------------------
// function prototype
//------------------------------------------------------------------------------
extern int usb_check    (int id);
extern int usb_rw       (int id);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif  // #define __USB_H__
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
