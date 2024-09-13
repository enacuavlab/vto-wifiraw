WFB : OK (rtl8812au already in riek)
WAYLAND ?
VPU : OK
GPU ?
CAMERA : Green image, missing rkaiq server
UART ?

---------------------------------------------------------------

!! BOARD WILL BURN WHEN USING USB-C CONNECTOR TO POWER 5V3A ... !!

---------------------------------------------------------------
https://github.com/Joshua-Riek/ubuntu-rockchip
https://github.com/Joshua-Riek/ubuntu-rockchip/wiki/
https://github.com/Joshua-Riek/ubuntu-rockchip/releases

---------------------------------------------------------------
v2.3.2

https://github.com/Joshua-Riek/ubuntu-rockchip/releases/download/.../ubuntu-24.04.3-preinstalled-server-arm64-radxa-zero3.img.xz

ubuntu-24.04.3-preinstalled-server-arm64-radxa-zero3.img.xz

----------------------------------------------------------------
cd /media/pprz/CIDATA/
"
network:
  version: 2
  ethernets:
    zz-all-en:
      match:
        name: "en*"
      link-local: [ ipv4 ]
      dhcp4: false
      dhcp6: false
      addresses:
      - 192.168.3.2/24
      nameservers:
        addresses:
        - 8.8.8.8
        - 8.8.4.4
      routes:
      - to: default
        via: 192.168.3.1
"

----------------------------------------------------------------
HDMI Display

First boot wait for ssh key generation (40 sec) AFTER login prompt
----END SSH HOST KEYS------

ssh ubuntu@192.168.3.2
ubuntu

=> change password
azertyuiop

ssh ubuntu@192.168.3.2
azertyuiop

sudo passwd ubuntu
pprz

-------------------------------------------------------------------------
https://github.com/Joshua-Riek/ubuntu-rockchip/wiki/Ubuntu-24.04-LTS

/etc/default/u-boot
"
U_BOOT_FDT_OVERLAYS="device-tree/rockchip/overlay/radxa-zero3-rpi-camera-v2.dtbo"
"
sudo u-boot-update
(/boot/extlinux/extlinux.conf)

reboot

/dev/video0 .. video9

------------------------------------------------------------------------
share wifi :

sudo iptables -t nat -A POSTROUTING -o wlp59s0 -j MASQUERADE
sudo iptables -A FORWARD -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
sudo iptables -A FORWARD -i enx3c18a0d60a31 -o wlp59s0 -j ACCEPT
sudo sysctl net.ipv4.ip_forward=1

-------------------------------------------------------------------------
wait
ssh ubuntu@192.168.3.2

sudo apt-get update
=>
Hit:2 http://ppa.launchpad.net/jjriek/rockchip/ubuntu jammy InRelease
...
Hit:4 http://ppa.launchpad.net/jjriek/rockchip-multimedia/ubuntu jammy InRelease

sudo apt upgrade

sudo apt-get install i2c-tools v4l-utils
v4l2-ctl -d /dev/video0 --list-formats-ext


sudo apt-get install gstreamer1.0-tools gstreamer1.0-plugins-good
sudo apt-get install gstreamer1.0-rockchip1

gst-launch-1.0 --version
=> 1.24.2

gst-inspect-1.0 rockchipmpp
=> mpph264enc, mpph265enc, mppvideodec


gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, width=1920, height=1080, framerate=30/1, format='NV12' ! mpph265enc rc-mode=vbr bps=3000000 bps-max=3172000  ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5600 host=192.168.3.1

gst-launch-1.0 udpsrc port=5600 ! application/x-rtp, encoding-name=H265, payload=96 ! rtph265depay ! h265parse ! queue ! avdec_h265 !  videoconvert ! autovideosink sync=false

-------------------------------------------------------------------------
sudo apt-get install gstreamer1.0-plugins-bad

gst-inspect-1.0 waylandsink
=> OK

sudo reboot

--------------------------------------------------
sudo apt install wayland-utils

sudo apt-get install weston
weston --version
=> 13.0.0

(sudo apt-get install mesa-utils)

systemd-run  --uid=1000 -p PAMName=login -p TTYPath=/dev/tty7 /usr/bin/weston
wayland-info
=> failed to create display: No such file or directory

#weston-info
#=> HDMI-A-1
#=> width: 2560 px, height: 1440 px, refresh: 59.951 Hz

mkdir .config
vi .config/weston.ini
"
[core]
idle-time=0

[shell]
#size=3840x2160
panel-location=""
panel-position=none
background-color=0xff000000

[output]
name=HDMI-A-1
mode=2560x1440@59.951
"

gst-launch-1.0 videotestsrc ! video/x-raw, width=2560, height=1440, framerate=30/1
! waylandsink display=/run/user/1000/wayland-1
=> CPU 80% (with or without GPU ?)

loginctl
loginctl kill-session

gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, width=1920, height=1080, framerate=30/1 ! queue ! waylandsink display=/run/user/1000/wayland-1
=>
NOTHING DISPLAYED !!


--------------------------------------------------
IMPROVE BOOT TIME
-----------------
snap list
=>
Name    Version      Rev    Tracking       Publisher   Notes
core22  20240823     1614   latest/stable  canonical✓  base
lxd     6.1-78a3d8f  30135  latest/stable  canonical✓  -
snapd   2.63         21761  latest/stable  canonical✓  snapd

sudo snap remove --purge lxd
sudo apt remove --purge snapd
sudo apt-mark hold snapd
sudo rm -R /root/snap

---------
sudo systemctl disable systemd-networkd-wait-online.service

sudo mkdir /etc/systemd/system/systemd-networkd-wait-online.service.d
sudo vi /etc/systemd/system/systemd-networkd-wait-online.service.d/override.conf
"
[Service]
ExecStart=
ExecStart=/lib/systemd/systemd-networkd-wait-online --any
"

--------------------------------------------------
INSTALL WFB
-----------
sudo apt-get install network-manager

nmcli radio wifi
=> enabled

sudo nmcli radio wifi off

reboot

nmcli radio wifi
=> disabled

sudo touch /etc/cloud/cloud-init.disabled

Plug Asus USB adpater AC56
dmesg -w
=> detected and driver rtl88XXau loaded
(might be from aircrack rtl8812au)

mkdir Projects; cd Projects
git clone --recurse-submodules  http://github.com/enacuavlab/vto-wifiraw.git
cd vto-wifiraw
comment rtl8812au driver installation in
install.sh

--------------------------------------------------
--------------------------------------------------
--------------------------------------------------
camera-engine-rkaiq-rk3588
no rk3566 !!

/etc/iqfiles/imx219_rpi-camera-v2_default.json

/usr/bin/rkaiq_3A_server
=>
DBG: get rkisp-isp-subdev devname: /dev/v4l-subdev0
DBG: get rkisp-input-params devname: /dev/video9
DBG: get rkisp-statistics devname: /dev/video8
DBG: get rkisp_mainpath devname: /dev/video0
rkaiq log level ff1
XCORE:K:rk_aiq_init_lib, ISP HW ver: 21
rkaiq_3A_server: ./rkaiq/./uAPI/rk_aiq_user_api_sysctl.cpp:1503: void rk_aiq_init_lib(): Assertion `g_rkaiq_isp_hw_ver == 30' failed.
Aborted



