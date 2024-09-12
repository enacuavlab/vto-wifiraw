This project provides wireless communication between ground and board systems.

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
ROLE := BOARD
#ROLE := GROUND commented !

make

src/Makefile
ROLE := GROUND
#ROLE := BOARD commented !

make

And set suitable video stream for BOARD in scripts/video.sh

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
nc -u -vv -l 5000

3) File transfert (tunnel mtu range 700,1400)
rsync -vP --bwlimit=5000  $USER@10.0.1.2:/tmp/100M.log .
(openssl rand 102400000 > /tmp/100M.log)

4) Video streaming (h265 parsing mtu range 400,1400)
gst-launch-1.0 udpsrc port=5600 ! application/x-rtp, encoding-name=H265, payload=96 ! \
rtph265depay ! h265parse ! queue ! avdec_h265 !  videoconvert ! autovideosink sync=false

5) Telemetry-datalink
link_py.py -d /dev/ttyUSB0 -t xbee -s 57600 -ac 122:127.0.0.1:4244:4245
(python3 -m pip install pyserial)

6) Payload-datalink
socat udp-listen:5800,reuseaddr,fork -
socat - udp-sendto:5900

etc ...

---------------------------------------------------------------------------------
0) How does it works
  Using a specific wireless adapter with injection and monitor capabilities, to build a bidirectionnal link.
  An additional wifi adapter on both ends, enable channel hopping, to avoid occupied frequencies.
  Moreover, the wifi driver can be patched to increase transmission power.

1) Behaviour
- On board: Todo
- On ground: Todo

2) Software development :
-  Used
  - netlink : to retreive available wifi channels and switch among them
  - raw socket : to retrieve full traffic and detect occupied channels
- Not used
  - fec : only recover partial information
  - pcap : not needed with raw socket

3) Context
- Wifi traffic => delays
  CSMA-CA (Collision Avoidance)
  - Carrier Sending
  - Carrier signal detected => wait Backoff period (Backoff: random delay from previous trials)
  - Carrier signal not detected => wait for free carrier during duration DIFS (Distributed Inter Frame Space)
  - Carrier signal not detected during DIFS => transmit data

4) Todo
- Available and used bandwitdh ?
- Latency ?

---------------------------------------------------------------------------------
Tested on Raspbian, Ubuntu/Debian and Nvidia Jetpack.  

2023-02-21-raspios-bullseye-arm64-lite, Raspberry PI3: Ground

Debian 11.3 (bulleyes) arm64 on Raspberry PI4,3,0V2: Board and Ground
 
Ubuntu 20.04 (server+desktop) arm64 on Raspberry PI4 : Ground with Paparazzi

Ubuntu 22.04 (desktop)

Jetpack 4.6 (arm64) on Nvidia Nano and XavierNX:  Board and Ground

