#!/bin/bash
STR="-------------------------------------------------------------------------\n"
STR+="This script will setup the communication system \n"
STR+="It must be run on ground and board sides \n\n"
STR+="After the installation copy the encryption keys (drone.key, gs.key) from the \n"
STR+="directory compagnon-software/wifiboradcast to have the same on both sides \n\n"
STR+="Turn off the genuine wifi before starting the system from a desktop \n"
STR+="The system, will start as soon as the USB dongle is inserted, or while booting \n"
STR+="with an already inserted dongle \n\n"
STR+="From the ground side you can check the communication (according the configuration):\n\n"
STR+="ssh pprz@10.0.1.2 \n\n"
STR+="or \n\n"
STR+="gst-launch-1.0 udpsrc port=5700 ! application/x-rtp, encoding-name=H264,payload=96 "
STR+="! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! autovideosink sync=false \n\n"
STR+="(sudo apt install -y gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-libav)\n\n" 
STR+="When using a Virtual machine for the installation, make sure you set \n"
STR+="Virtual Machine Setting / Hardware / USB Controller / USB compatibility USB 3.1 \n\n"
STR+="-------------------------------------------------------------------------\n"
STR+="Do you want to proceed with the installation (Internet and sudo required) " 
echo -e $STR
USER=`basename $PWD`
PROJ=$PWD
read -p "for this side in $PROJ (y/n) ?" ANSWER
if [ ! $ANSWER = "y" ] || [ -z $ANSWER ]; then exit -1; fi
if ! groups | grep -q 'sudo'; then exit -1; fi
sudo apt-get install -y socat git net-tools wireless-tools rfkill v4l-utils
cd $PROJ/rtl8812au
git apply ../material/rtl8812au_v5.6.4.2.patch
DKMS=false
if uname -a | grep -cs "Ubuntu"> /dev/null 2>&1;then DKMS=true; fi
if uname -a | grep -cs "4\.9\.201-tegra"> /dev/null 2>&1;then DKMS=true; fi
if uname -a | grep -cs "4\.9\.140"> /dev/null 2>&1;then DKMS=true; fi
if uname -a | grep -cs "4.9.253-tegra"> /dev/null 2>&1;then DKMS=true; fi
if $DKMS; then
  echo "blacklist rtl8812au" |sudo tee -a /etc/modprobe.d/blacklist.conf > /dev/null 2>&1
  sudo apt-get install -y dkms
  sudo make dkms_install
else 
  if uname -a | grep -cs "aarch64"; then
    # RPI 02 ,3B+ & 4 (with OS 64b)
    sed -i 's/CONFIG_PLATFORM_I386_PC = y/CONFIG_PLATFORM_I386_PC = n/g' Makefile
    sed -i 's/CONFIG_PLATFORM_ARM64_RPI = n/CONFIG_PLATFORM_ARM64_RPI = y/g' Makefile
  else
    #  RPI 0 & 3 & 4 (with OS 32)
    sed -i 's/CONFIG_PLATFORM_I386_PC = y/CONFIG_PLATFORM_I386_PC = n/g' Makefile
    sed -i 's/CONFIG_PLATFORM_ARM_RPI = n/CONFIG_PLATFORM_ARM_RPI = y/g' Makefile
  fi
  sudo apt-get install linux-headers
  make
  sudo make install
fi  
cd $PROJ/wfb-ng
sudo apt-get install -y libpcap-dev libsodium-dev
make all_bin
#make gs.key
rm -Rf $PROJ/patched
mkdir $PROJ/patched
cp $PROJ/scripts/air.sh $PROJ/patched
cp $PROJ/scripts/ground.sh $PROJ/patched
cp $PROJ/scripts/wfb_on.sh $PROJ/patched
cp $PROJ/scripts/wifibroadcast.service $PROJ/patched
sed -i 's|compagnonsoftwarepath|'"$PROJ"'|g' $PROJ/patched/air.sh
sed -i 's|compagnonsoftwarepath|'"$PROJ"'|g' $PROJ/patched/ground.sh
sed -i 's|compagnonsoftwarepath|'"$PROJ"'|g' $PROJ/patched/wfb_on.sh
sed -i 's|compagnonsoftwarepath|'"$PROJ"'|g' $PROJ/patched/wifibroadcast.service
sudo cp $PROJ/material/rtl8812au.conf /etc/modprobe.d
if ! uname -a | grep -cs "4.9.201-tegra"> /dev/null 2>&1; 
  then sudo sh -c "echo 'options 88XXau rtw_switch_usb_mode=1' >> /etc/modprobe.d/rtl8812au.conf"; fi
sudo cp $PROJ/patched/wifibroadcast.service /etc/systemd/system
sudo cp $PROJ/material/60-wifibroadcast.rules /etc/udev/rules.d
sudo udevadm control --reload-rules
sudo systemctl enable wifibroadcast.service
sudo systemctl start wifibroadcast.service
sudo systemctl daemon-reload
if ! $DKMS; then 
  sudo sh -c "echo 'denyinterfaces wlan1' >> /etc/dhcpcd.conf"
fi
