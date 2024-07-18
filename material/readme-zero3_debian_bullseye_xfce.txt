DEBIAN 11

https://github.com/radxa-build/radxa-zero3/releases/download/b6/radxa-zero3_debian_bullseye_xfce_b6.img.xz

Radxa First Boot Configuration

config/before.txt
"
connect_wi-fi Androidxp pprzpprz
"
--------------------------------------------------------------
poweron
=> blinking led

--------------------------------------------------------------
Ethernet connection
-------------------
static ip shall be configured with desktop GUI
192.168.3.2
255.255.255.0
192.168.3.1

ssh rock@192.168.3.2
rock

--------------------------------------------------------------
Wific connection
-------------------
ip address
=> 192.168.1.131
sudo nmap -sn 192.168.1.131/24
=> Nmap scan report for radxa-zero3.home (192.168.1.87)

ssh rock@192.168.1.87
rock

--------------------------------------------------------------
sudo apt-get update

sudo rsetup
Overlays
Manage overlay
Enable Raspberry Pi Camera v2
Enable UART4-M1

sudo reboot
ssh rock@...

sudo apt remove xfce4
sudo apt remove xserver-xorg
sudo apt autoremove

sudo reboot
ssh rock@...

--------------------------------------------------------------
gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, width=640, height=480, framerate=30/1 ! mpph265enc ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5600 host=192.168.1.131
(csi imx219 + usb webcam = OK)

gst-launch-1.0 udpsrc port=5600 ! application/x-rtp, encoding-name=H265, payload=96 ! rtph265depay ! h265parse ! queue ! avdec_h265 !  videoconvert ! autovideosink sync=false 

--------------------------------------------------------------
git clone --recurse-submodules  http://github.com/enacuavlab/vto-wifiraw.git

vi vto-wifiraw/install.sh
"
if uname -a | grep -cs "radxa-zero3"> /dev/null 2>&1;then DKMS=true; fi
"
./install.sh

vi vto-wifiraw/src/Makefile
"
  ifeq ($(UNAME_R),5.10.160-26-rk356x)
    OSFLAG = -DUART=\"/dev/ttyS2\"
"

sudo apt-get install build-essential

/usr/sbin/iwconfig

sudo ip link set wlan0 down

sudo ./wfb
=> temp=64

--------------------------------------------------------------
v4l2-ctl -d /dev/video9 --list-formats-ext


IMX219

gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, width=1280, height=720, framerate=30/1 ! mpph265enc rc-mode=vbr bps=3000000 bps-max=3172000  ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5600 host=192.168.84.194

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

--------------------------------------------------------------
--------------------------------------------------------------
sudo vi /etc/NetworkManager/NetworkManager.conf
"
[ifupdown]
managed=false
"

sudo vi /etc/NetworkManager/system-connections/Wired.nmconnection
"
[connection]
id=Wired connection 1
#uuid=18b1c20b-e2f5-356c-9d12-707550d6ab47
type=ethernet
autoconnect-priority=-999
interface-name=enx3c18a0d60afa
#interface-name=enxd0c0bf2f6e0b
permissions=
timestamp=1687100327

[ethernet]
mac-address-blacklist=

[ipv4]
address1=192.168.3.2/24,192.198.3.1
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
Lemorele adapteur:
interface-name=enxd0c0bf2f6e0b

--------------------------------------------------------------
sudo sysctl -w net.ipv6.conf.all.disable_ipv6=1

--------------------------------------------------------------

/usr/sbin/ifconfig
=> wlan0

sudo ip link set dev wlan1 down

sudo iw wlxfc34972ed57c set type managed;sudo iw wlx7c10c91c408e set type managed;sudo ip link set wlxfc34972ed57c down;sudo ip link set wlx7c10c91c408e down

sudo iw wlxfc34972ed57c set type monitor
sudo ip link set wlxfc34972ed57c up
sudo iw dev wlxfc34972ed57c set channel 13

sudo iw wlx7c10c91c408e set type monitor
sudo ip link set wlx7c10c91c408e up
sudo iw dev wlx7c10c91c408e set channel 84

clear;socat udp-listen:5000,reuseaddr,fork - | tee stdout.log


sudo iw wlx68ca00012942 set type managed;sudo iw wlx68ca0001294c set type managed;sudo ip link set wlx68ca00012942 down;sudo ip link set wlx68ca0001294c down

sudo iw wlx68ca00012942 set type monitor
sudo ip link set wlx68ca00012942 up
sudo iw dev wlx68ca00012942 set channel 0

