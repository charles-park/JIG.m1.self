//------------------------------------------------------------------------------
/**
 * @file hdmi.h
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
#ifndef __HDMI_H__
#define __HDMI_H__

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Define the Device ID for the HDMI group.
//------------------------------------------------------------------------------
enum {
    eHDMI_EDID,
    eHDMI_HPD,
    eHDMI_END
};

//------------------------------------------------------------------------------
// function prototype
//------------------------------------------------------------------------------
extern int hdmi_check     (int id);

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif  // #define __HDMI_H__
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
