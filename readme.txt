This project provides wireless interface between ground and board systems.

Two wireless devices on both sides may improve communication using rolling channel hopping.

-------------------------------------------------------------------------------
git clone --recurse-submodules  http://github.com/enacuavlab/vto-wifiraw.git
(git clone --recurse-submodules  git@github.com:enacuavlab/vto-wifiraw.git)

Installation need internet connection and sudo 

cd vto-wifiraw
./install.sh
or
./uninstall.sh

-------------------------------------------------------------------------------
Before first run, compile the suitable executable file on BOARD and on GROUND

src/Makefile
#ROLE := BOARD commented !
ROLE := GROUND

make

src/Makefile
ROLE := BOARD
#ROLE := GROUND commented !

make

-------------------------------------------------------------------------------
Usages:
------
0) 
sudo apt install openssh-server
sudo systemctl enable ssh

1) Remote shell
ssh $USER@10.0.1.2

2) Wfb logs
socat udp-listen:5000,reuseaddr,fork -

3) File transfert (tunnel mtu range 700,1400)
rsync -vP --bwlimit=5000  $USER@10.0.1.2:/tmp/100M.log .
(openssl rand 102400000 > /tmp/100M.log)

4) Video streaming (h264 parsing mtu range 400,1400)
gst-launch-1.0 udpsrc port=5600 ! application/x-rtp, encoding-name=H264, payload=96 ! \
rtph264depay ! h264parse ! queue ! avdec_h264 !  videoconvert ! autovideosink sync=false

5) Telemetry-datalink
link_py.py -d /dev/ttyUSB0 -t xbee -s 57600 -ac 122:127.0.0.1:4244:4245

6) Payload-datalink
socat udp-listen:5800,reuseaddr,fork -
socat - udp-sendto:5900

etc ...

---------------------------------------------------------------------------------
Tested on Raspbian, Ubuntu/Debian and Nvidia Jetpack.  

2023-02-21-raspios-bullseye-arm64-lite, Raspberry PI3: Ground

Debian 11.3 (bulleyes) arm64 on Raspberry PI4,3,0V2: Board and Ground
 
Ubuntu 20.04 (server+desktop) arm64 on Raspberry PI4 : Ground with Paparazzi

Ubuntu 22.04 (desktop)

Jetpack 4.6 (arm64) on Nvidia Nano and XavierNX:  Board and Ground

