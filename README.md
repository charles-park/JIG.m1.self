# JIG.m1.self
ODROID-M1(RK3568) Self-test jig source (m1-server upgrade version, New spi-flash support)

### Documentation for JIG development
* Document : https://docs.google.com/spreadsheets/d/18J4B4bqgUbMBA8XeoDkMcKPVEAeNCP2jQm5TOlgKUAo/edit?gid=1741005235#gid=1741005235

### Image used for testing.
* odroidh3/생산관리/jig/odroid-m1.new/uboot-include-ubuntu-odroid-m1.img
* odroidh3/생산관리/jig/odroid-m1.new/spiupdate_odroidm1_20240529.img

### Update Image info (apt update && apt upgrade -y)
 * U-Boot : U-Boot 2017.09 (May 28 2024 - 10:23:38 +0000)
 * Kernel : Linux server 4.19.219-odroid-arm64 #1 SMP Mon, 24 Jun 2024 04:52:55 +0000 aarch64 aarch64 aarch64
 GNU/Linux

### pettiboot skip
* Create a petitboot.cfg file in /boot to skip petitboot.
```
root@server:~# vi /boot/petitboot.cfg
[petitboot]
petitboot,timeout=0
```

### Self mode settings
* Install ubuntu package & python3 module
```
// odroid server repo add
root@server:~# echo "192.168.0.224 ppa.linuxfactory.or.kr" >> /etc/hosts
root@server:~# echo "deb  http://ppa.linuxfactory.or.kr focal internal" >> /etc/apt/sources.list.d/ppa-linuxfactory-or-kr.list

// ubuntu system update
root@server:~# apt update --fix-missing
root@server:~# apt update && apt upgrade -y

// ubuntu package
root@server:~# apt install samba ssh build-essential python3 python3-pip ethtool net-tools usbutils git i2c-tools vim cups cups-bsd overlayroot nmap iperf3 alsa-utils

// python3 package
root@server:~# pip install aiohttp asyncio

// git config
root@server:~# git config --global user.email "charles.park@hardkenrel.com"
root@server:~# git config --global user.name "charles-park"
root@server:~# git config --global core.editor "vim"

```

* Edit the /boot/config.ini, HDMI Resolution (1080p), Header40 All gpio mode.
```
root@server:~# vi /boot/config.ini

[generic]
# Added. Vu7 Resolution setup
resolution=1920x1080
refresh=60

overlay_resize=16384
overlay_profile=""
# default
# overlays="uart0 i2c0 i2c1"

# modified
overlays=""

[overlay_custom]
overlays="i2c0 i2c1"

[overlay_hktft32]
overlays="hktft32"

[overlay_hktft35]
overlays="hktft35 sx865x-i2c1"
```
```
// Boot script update (HDMI resolution control)
root@server:~# cp -r /usr/share/flash-kernel/preboot.d/vendor /usr/share/flash-kernel/preboot.d/upstream
root@server:~# update-bootscript
```

### Clone the reopsitory with submodule
```
root@odroid:~# git clone --recursive https://github.com/charles-park/JIG.m1.self

or

root@odroid:~# git clone https://github.com/charles-park/JIG.m1.self
root@odroid:~# cd JIG.m1.self
root@odroid:~/JIG.m1.self# git submodule update --init --recursive

// app build and all package install
root@odroid:~/JIG.m1.self# ./install

```

### add root user, ssh root enable (https://www.leafcats.com/176)
```
// root user add
root@server:~# passwd root

root@server:~# vi /etc/ssh/sshd_config

...
# PermitRootLogin prohibit-password
PermitRootLogin yes
StrictModes yes
PubkeyAuthentication yes
...

// ssh daemon restart
root@server:~# service sshd restart
```

### auto login
```
root@server:~# systemctl edit getty@tty1.service
```
```
[Service]
ExecStart=
ExecStart=-/sbin/agetty --noissue --autologin root %I $TERM
Type=idle
```

### samba config
```
root@server:~# smbpasswd -a root
root@server:~# vi /etc/samba/smb.conf
```
```
[odroidm2-self]
   comment = odroid-m1s jig root
   path = /root
   guest ok = no
   browseable = yes
   writable = yes
   create mask = 0775
   directory mask = 0775
```
```
// samba restart
root@server:~# service smbd restart
```

### Sound setup
```
root@server:~# aplay -l
**** List of PLAYBACK Hardware Devices ****
card 0: ODROIDM1HDMI [ODROID-M1-HDMI], device 0: fe400000.i2s-i2s-hifi i2s-hifi-0 [fe400000.i2s-i2s-hifi i2s-hifi-0]
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 1: ODROIDM1FRONT [ODROID-M1-FRONT], device 0: fe410000.i2s-rk817-hifi rk817-hifi-0 [fe410000.i2s-rk817-hifi rk817-hifi-0]
  Subdevices: 1/1
  Subdevice #0: subdevice #0

root@server:~# amixer -c 1
Simple mixer control 'Playback Path',0
  Capabilities: enum
  Items: 'OFF' 'RCV' 'SPK' 'HP' 'HP_NO_MIC' 'BT' 'SPK_HP' 'RING_SPK' 'RING_HP' 'RING_HP_NO_MIC' 'RING_SPK_HP'
  Item0: 'OFF'
Simple mixer control 'Capture MIC Path',0
  Capabilities: enum
  Items: 'MIC OFF' 'Main Mic' 'Hands Free Mic' 'BT Sco Mic'
  Item0: 'MIC OFF'

root@server:~# amixer -c 1 sset 'Playback Path' 'HP'
root@server:~# amixer -c 1
Simple mixer control 'Playback Path',0
  Capabilities: enum
  Items: 'OFF' 'RCV' 'SPK' 'HP' 'HP_NO_MIC' 'BT' 'SPK_HP' 'RING_SPK' 'RING_HP' 'RING_HP_NO_MIC' 'RING_SPK_HP'
  Item0: 'HP'
Simple mixer control 'Capture MIC Path',0
  Capabilities: enum
  Items: 'MIC OFF' 'Main Mic' 'Hands Free Mic' 'BT Sco Mic'
  Item0: 'MIC OFF'
```
* Sound test (Sign-wave 1Khz)
```
// use speaker-test
root@server:~# speaker-test -D hw:0,0 -c 2 -t sine -f 1000           # pin header target, all
root@server:~# speaker-test -D hw:0,0 -c 2 -t sine -f 1000 -p 1 -s 1 # pin header target, left
root@server:~# speaker-test -D hw:0,0 -c 2 -t sine -f 1000 -p 1 -s 2 # pin header target, right

// or use aplay with (1Khz audio file)
root@server:~# aplay -Dhw:1,0 {audio file} -d {play time}
```

### Overlay root
* overlay-root enable
```
root@odroid:~# update-initramfs -c -k $(uname -r)
update-initramfs: Generating /boot/initrd.img-4.9.277-75
root@odroid:~#
root@odroid:~# mkimage -A arm64 -O linux -T ramdisk -C none -a 0 -e 0 -n uInitrd -d /boot/initrd.img-$(uname -r) /boot/uInitrd 
Image Name:   uInitrd
Created:      Wed Feb 23 09:31:01 2022
Image Type:   AArch64 Linux RAMDisk Image (uncompressed)
Data Size:    13210577 Bytes = 12900.95 KiB = 12.60 MiB
Load Address: 00000000
Entry Point:  00000000
root@odroid:~#

overlayroot.conf 파일의 overlayroot=””를 overlayroot=”tmpfs”로 변경합니다.
vi /etc/overlayroot.conf
overlayroot_cfgdisk="disabled"
overlayroot="tmpfs"
```
* overlay-root modified/disable  
```
[get write permission]
odroid@hirsute-server:~$ sudo overlayroot-chroot 
INFO: Chrooting into [/media/root-ro]
root@hirsute-server:/# 

[disable overlayroot]
overlayroot.conf 파일의 overlayroot=”tmpfs”를 overlayroot=””로 변경합니다.
vi /etc/overlayroot.conf
overlayroot_cfgdisk="disabled"
overlayroot=""

```
