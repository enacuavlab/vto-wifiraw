DEBIAN 11

https://github.com/radxa-build/radxa-zero3/releases/download/b6/radxa-zero3_debian_bullseye_xfce_b6.img.xz

Radxa First Boot Configuration

config/before.txt
"
connect_wi-fi Androidxp pprzpprz
"

--------------------------------------------------------------
Headless install

--------------------------------------------------------------
First boot with wifi to set static ethernet (Desktop static 192.168.3.1)
=> blinking led

--------------------------------------------------------------
Wifi connection
-------------------
ip address
=> 192.168.1.131
sudo nmap -sn 192.168.1.131/24
=> Nmap scan report for radxa-zero3.home (192.168.1.87)

ssh rock@192.168.1.87
rock

--------------------------------------------------------------
sudo nmtui
"
Profile name Ethernetconnection1
Device 
IPV4 CONFIGURATION <Manual>
Addresse 192.168.3.2/24
Gateway 192.168.3.1
DNS Servers 8.8.8.8
            8.8.4.4            
IPV6 CONFIGURATION <Disabled>     
"

sudo nmtui
=> activate

reboot
ssh rock@192.168.3.2
rock

sudo nmcli connection show -a
=>
Wired connection 1  d4bef1de-dde8-3892-b8c5-3f42a20b2c3d  ethernet  enxd0c0bf2f6e0b 
Androidxp           5f35dea4-6a28-4033-b2bb-f6f2801c40a9  wifi      wlan0           

sudo nmcli device status
=>
enxd0c0bf2f6e0b  ethernet  connected     Wired connection 1 
wlan0            wifi      connected     Androidxp    

sudo nmcli radio wifi off
sudo nmcli device status
=>
enxd0c0bf2f6e0b  ethernet  connected    Wired connection 1 
wlan0            wifi      unavailable  --                 
p2p-dev-wlan0    wifi-p2p  unavailable  --                 
usb0             ethernet  unmanaged    --                 
lo               loopback  unmanaged    --    

--------------------------------------------------------------
Share wifi :

sudo iptables -t nat -A POSTROUTING -o wlp59s0 -j MASQUERADE
sudo iptables -A FORWARD -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
sudo iptables -A FORWARD -i enx3c18a0d60a31 -o wlp59s0 -j ACCEPT
sudo sysctl net.ipv4.ip_forward=1

ssh rock@192.168.3.2
rock

sudo apt-get update

sudo rsetup
Overlays
Manage overlay
Enable Raspberry Pi Camera v2
Enable UART4-M1

sudo apt remove xfce4
sudo apt remove xserver-xorg
sudo apt autoremove

--------------------------------------------------------------
gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, width=640, height=480, framerate=30/1 ! mpph265enc ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5600 host=192.168.3.1
(csi imx219 + usb webcam = OK)

gst-launch-1.0 udpsrc port=5600 ! application/x-rtp, encoding-name=H265, payload=96 ! rtph265depay ! h265parse ! queue ! avdec_h265 !  videoconvert ! autovideosink sync=false 

--------------------------------------------------------------
uname -a
Linux radxa-zero3 5.10.160-26-rk356x #bfb9351f3 SMP Wed Jan 10 07:01:50 UTC 2024 aarch64 GNU/Linux

git clone --recurse-submodules  http://github.com/enacuavlab/vto-wifiraw.git
cd vto-wifiraw
./install.sh

plug usb rtl8812au wifi adpater and check 
/sbin/iwconfig

--------------------------------------------------------------
v4l2-ctl -d /dev/video9 --list-formats-ext


IMX219

gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, width=1920, height=1080, framerate=30/1, format='NV12' ! mpph265enc ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5600 host=127.0.0.1

gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, width=1280, height=720, framerate=30/1 ! mpph265enc rc-mode=vbr bps=3000000 bps-max=3172000  ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5600 host=127.0.0.1

gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, width=1920, height=1080, framerate=30/1, format='NV12' ! mpph265enc rc-mode=vbr bps=3000000 bps-max=3172000  ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5600 host=127.0.0.1



JEVOIS
(POWER !)
gst-launch-1.0 v4l2src device=/dev/video9 ! video/x-raw, width=1280, height=740, framerate=10/1 ! videoscale ! video/x-raw, width=1280, height=736, framerate=10/1 ! mpph265enc ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5600 host=192.168.84.194

WEBCAM

gst-launch-1.0 v4l2src device=/dev/video11 ! video/x-raw, width=640, height=480, framerate=30/1 ! mpph265enc rc-mode=vbr bps=3000000 bps-max=3172000  ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5600 host=192.168.84.194


GROUND

gst-launch-1.0 udpsrc port=5600 ! application/x-rtp, encoding-name=H265, payload=96 ! rtph265depay ! h265parse ! queue ! avdec_h265 !  videoconvert ! autovideosink sync=false

ssh rock@10.0.1.2
htop
=> 20% cpu


----------------------------------------------------------------------------------------------
USING FREE UART TO AUTOPILOT (PPRZ)
-------------------------------------------
ls -la /dev/ttyS4

sudo socat /dev/ttyS4,b115200 udp4-datagram:192.168.3.1:4244
sudo socat udp4-datagram:192.168.3.1:4245 /dev/ttyS4,b115200


(
python3 -m pip install pyserial
/home/pprz/Projects/paparazzi/sw/ground_segment/tmtc/link -d /dev/ttyUSB0 -transport xbee -s 57600
/home/pprz/Projects/paparazzi/sw/ground_segment/tmtc/link_py.py -d /dev/ttyUSB0 -t xbee -s 57600 -ac 115:127.0.0.1:4244:4245
/home/pprz/Projects/paparazzi/sw/ground_segment/tmtc/messages
)

-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
USING EMMC 
----------------------------------------------------------------
1) From operational micro sd build and set above extract file image
--------------
sudo gparted /dev/sdb
shrink
sudo gdisk -l /dev/sdb
=>
Sector size (logical/physical): 512/512 bytes
...
Number  Start (sector)    End (sector)  Size       Code  Name
   1           32768           65535   16.0 MiB    0700  config
   2           65536          679935   300.0 MiB   EF00  boot
   3          679936        11149311   5.0 GiB     EF00  rootfs

11149311 + 1 + 33 = 11149345 (size including GPT tail header)

sudo dd if=/dev/sdb of=sd.img bs=512 count=11149345 status=progress
=>
5708464640 bytes (5,7 GB, 5,3 GiB) copied, 378,95 s, 15,1 MB/s
5,4G sd.img

sudo gdisk sd.img
...
  GPT: damaged

Command
r : Recovery/transformation command
d : use main GPT header (rebuilding backup)
w : write table to disk and exit
Y

sudo gdisk sd.img
...
  GPT: present

sudo gparted sd.img

BalenaEtcher sd.img to new /dev/sdb

sudo gparted

Not all of the space available to /dev/sdb appears to be used, 
you can fix the GPT to use all of the space (an extra 51371999 blocks) 
or continue with the current setting? 

Continue

-------------------------------------------------------------------------------
2) From extract file image build new SD containing file image to be written in EMMC
--------------
gparted /dev/sdb
Fix size
Encrease size to host image (+ 5Gb)
Copy image in /dev/sdb

move SD in Zero3W and boot
ssh rock@192.168.3.2
lsblk
=>
mmcblk1
mmcblk0

sudo dd if=sd.img of=/dev/mmcblk0 bs=512  status=progress
=>
5708464640 bytes (5.7 GB, 5.3 GiB) copied, 536.468 s, 10.6 MB/s

sudo poweroff
remove SD

-------------------------------------------------------------------------------
3) Extend EMMC
--------
ssh rock@192.168.3.2
sudo parted
=> Using /dev/mmcblk0
print
Warning: Not all of the space available to /dev/mmcblk0 appears to be used,
you can fix the GPT to use all of thespace (an extra 19386335 blocks)
or continue with the current setting?
Fix/Ignore?
Fix

sudo parted /dev/mmcblk0 print free
=>
Number  Start   End     Size    File system  Name    Flags
        17.4kB  16.8MB  16.8MB  Free Space
 1      16.8MB  33.6MB  16.8MB  fat32        config  msftdata
 2      33.6MB  348MB   315MB   fat32        boot    boot, esp
 3      348MB   5708MB  5360MB  ext4         rootfs  boot, esp
        5708MB  15.6GB  9926MB  Free Space


sudo parted /dev/mmcblk0 resizepart
Partition number? 3
Warning: Partition /dev/mmcblk0p3 is being used. Are you sure you want to continue?
Yes/No? yes
End?  [5708MB]? 15.6GB
Information: You may need to update /etc/fstab.

sudo parted /dev/mmcblk0 print free
=>
...
 3      348MB   15.6GB  15.3GB  ext4         rootfs  boot, esp

sudo resize2fs  /dev/mmcblk0p3
df -h
=>
/dev/mmcblk0p3   14G  4.1G  9.3G  31% /

-------------------------------------------------------------------------------
3) Boot from SD or EMMC
-----------------------
If EMMC is bootable it will boot from EMMC and not SD.
To force boot from SD, make EMMC unbootable.

sudo rkdeveloptool ef
Starting to erase flash...
Getting flash info from device failed!

without SD

lsblk
=> mmcblk0

sudo dd if=/dev/zero of=/dev/mmcblk0 bs=1M count=1000 status=progress
=>
1044381696 bytes (1.0 GB, 996 MiB) copied, 7 s, 149 MB/s

poweroff
Plug SD
poweron

-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
-------------------------------------------------------------------------------
Trying to display (but not priority)


sudo apt-get install cmake build-essential pkg-config ninja-build
sudo pip3 install meson

mkdir -p ~/RGA && cd ~/RGA
git clone -b linux-rga-multi --depth=1 https://github.com/JeffyCN/mirrors.git rkrga
meson setup rkrga rkrga_build \
    --prefix=/usr \
    --libdir=lib \
    --buildtype=release \
    --default-library=shared \
   -Dcpp_args=-fpermissive \
   -Dlibdrm=false \
   -Dlibrga_demo=false
sudo ninja -C rkrga_build install
sudo ldconfig
=> /dev/rga

# sudo apt-get install libxkbcommon-dev
# git clone https://github.com/JeffyCN/weston.git
# cd weston
# PATH=$PATH:/home/rock/.local/bin
# meson setup build

sudo apt-get install weston
weston --version
=> 9.0.0

systemd-run  --uid=1000 -p PAMName=login -p TTYPath=/dev/tty7 /usr/bin/weston
