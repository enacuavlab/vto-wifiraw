PI flash & headless configure
-----------------------------
PI4
2022-09-22-raspios-bullseye-arm64-lite.img.xz

PI3
2023-02-21-raspios-bullseye-arm64-lite.img.xz

-------------------------------------------------------------------------------------------
cd /media/.../boot
sudo touch ssh

sudo vi wpa_supplicant.conf
"
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1
country=FR
network={
  ssid="Androidxp"
  psk="pprzpprz"
}
network={
  ssid="pprz_router"
  key_mgmt=NONE
  }
"

-------------------------------------------------------------------------------------------
sudo vi /media/../rootfs/etc/dhcpcd.conf
Add "denyinterfaces wlan1" to the end of the file (but above any other added interface lines).
"
denyinterfaces wlan1
# It is possible to fall back to a static IP if DHCP fails:
# define static profile
profile static_eth0
static ip_address=192.168.3.2/24
static routers=192.168.3.1
static domain_name_servers=8.8.8.8
# fallback to static profile on eth0
interface eth0
fallback static_eth0
"

-------------------------------------------------------------------------------------------
/boot/config.txt
dtoverlay=pi3-disable-bt

sudo reboot

sudo systemctl stop serial-getty@ttyAMA0.service
sudo systemctl disable serial-getty@ttyAMA0.service

sudo systemctl stop hciuart
sudo systemctl disable hciuart

-------------------------------------------------------------------------------------------
/boot/userconf

username:password-hash

echo "pprz" | openssl passwd -6 -stdin
pi:$6$38HiUnL.... 

