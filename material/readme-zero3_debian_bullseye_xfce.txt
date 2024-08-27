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
sudo vi /etc/NetworkManager/NetworkManager.conf
"
[ifupdown]
managed=false
"

sudo vi /etc/NetworkManager/system-connections/Wired.nmconnection
"
[connection]
id=Wired connection1
type=ethernet
permissions=
timestamp=1721292972

[ethernet]
mac-address-blacklist=

[ipv4]
address1=192.168.3.2/24,192.168.3.1
dns=8.8.8.8;8.8.4.4;
dns-search=
method=manual

[ipv6]
addr-gen-mode=stable-privacy
dns-search=
method=disabled

[proxy]
"

--------------------------------------------------------------
sudo nmtui
=> activate

reboot

sudo nmcli connection show -a
=>
Wired connection 1  d4bef1de-dde8-3892-b8c5-3f42a20b2c3d  ethernet  enxd0c0bf2f6e0b 
Androidxp           5f35dea4-6a28-4033-b2bb-f6f2801c40a9  wifi      wlan0           

sudo nmcli device status
=>
enxd0c0bf2f6e0b  ethernet  connected     Wired connection 1 
wlan0            wifi      connected     Androidxp    

--------------------------------------------------------------
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
sudo reboot
ssh rock@192.168.3.2

sudo nmtui
change ethernet to enx3c18a0d60afa

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
sudo mount /dev/sdb3 /media/pprz/rootfs/
cd /media/pprz/rootfs/etc/NetworkManager/system-connections
sudo vi Wired connection 1.nmconnection
"
uuid=d4bef1de-dde8-3892-b8c5-3f42a20b2c3d
interface-name=enx3c18a0d60afa
"
to
"
#uuid=d4bef1de-dde8-3892-b8c5-3f42a20b2c3d
interface-name=enxd0c0bf2f6e0b
"

cd /media/pprz
sudo tar cvf roofs.tar rootfs/*
=> 5G
sudo mv roofs.tar /media/pprz/rootfs/home/rock
cd 
sudo umount /media/pprz/*
sync

Boot
ssh rock@192.168.3.2
rock

-------------------------------------------------------------------------------
sudo gparted 
resize sdb3

lsblk
mmcblk1      179:0    0  29.7G  0 disk 
├─mmcblk1p1  179:1    0    16M  0 part /config
├─mmcblk1p2  179:2    0   300M  0 part /boot/efi
└─mmcblk1p3  179:3    0   5.6G  0 part /
mmcblk0      179:32   0  14.6G  0 disk 
mmcblk0boot0 179:64   0     4M  1 disk 
mmcblk0boot1 179:96   0     4M  1 disk 
zram0        254:0    0 991.8M  0 disk [SWAP]


rock@radxa-zero3:~$ 



tar xvf root.tar /dev/



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
