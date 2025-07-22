#!/bin/bash
STR="-------------------------------------------------------------------------\n"
STR+="This script will setup the communication system \n"
STR+="It must be run on ground and board sides \n\n"
STR+="The system, will start as soon as the USB dongle is inserted, or while booting \n"
STR+="with an already inserted dongle \n\n"
STR+="From the ground side you can check the communication (according the configuration):\n\n"
STR+="ssh pprz@10.0.1.2 \n\n"
STR+="-------------------------------------------------------------------------\n"
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
sudo apt-get install -y netcat socat git net-tools wireless-tools rfkill v4l-utils build-essential

cd $PROJ/rtl8812au
if uname -a | grep -cs "6.14.0-24-generic"> /dev/null 2>&1; then git checkout v5.6.4.2;
else git checkout 63cf0b4; fi

#if uname -a | grep -cs "4.9.253-tegra"> /dev/null 2>&1; then git checkout 4ab079f7; fi
#git apply ../material/rtl8812au_v5.6.4.2.patch
DKMS=false
if uname -a | grep -cs "Ubuntu"> /dev/null 2>&1; then DKMS=true; fi
if uname -a | grep -cs "4.9.253-tegra"> /dev/null 2>&1; then DKMS=true; fi
if uname -a | grep -cs "5.10.160-legacy-rk35xx"> /dev/null 2>&1; then DKMS=true; fi
if uname -a | grep -cs "radxa-zero3"> /dev/null 2>&1; then DKMS=true; fi
if uname -a | grep -cs "6.1.0-1012-rockchip"> /dev/null 2>&1; then 
  DKMS=true
  sudo rm /usr/lib/modules/6.1.0-1012-rockchip/kernel/drivers/net/wireless/rtl8812au/88XXau.ko
fi
if $DKMS; then
  echo "blacklist rtl8812au" |sudo tee -a /etc/modprobe.d/blacklist.conf > /dev/null 2>&1
  sudo apt-get install -y dkms
  sudo make dkms_install
else 
  echo "NOT SUPPORTED YET ... exit !"
  exit
#  if uname -a | grep -cs "aarch64"; then
#    # RPI 02 ,3B+ & 4 (with OS 64b)
#    sed -i 's/CONFIG_PLATFORM_I386_PC = y/CONFIG_PLATFORM_I386_PC = n/g' Makefile
#    sed -i 's/CONFIG_PLATFORM_ARM64_RPI = n/CONFIG_PLATFORM_ARM64_RPI = y/g' Makefile
#  else
#    #  RPI 0 & 3 & 4 (with OS 32)
#    sed -i 's/CONFIG_PLATFORM_I386_PC = y/CONFIG_PLATFORM_I386_PC = n/g' Makefile
#    sed -i 's/CONFIG_PLATFORM_ARM_RPI = n/CONFIG_PLATFORM_ARM_RPI = y/g' Makefile
#  fi
#  if uname -r | grep -cs "5.15.61-v8+"; then
#    wget https://archive.raspberrypi.org/debian/pool/main/r/raspberrypi-firmware/raspberrypi-kernel-headers_1.20220830-1_arm64.deb
#    sudo apt-get install ./raspberrypi-kernel-headers_1.20220830-1_arm64.deb
#  else
#    sudo apt-get install linux-headers
#  fi
#  make
#  sudo make install
fi  
ln -s $PROJ/zfec/zfec/fec.c $PROJ/src
ln -s $PROJ/zfec/zfec/fec.h $PROJ/src
sudo apt-get install libnl-3-dev
if uname -m | grep -Eq 'x86_64' ; then
  sudo ln -s /usr/lib/x86_64-linux-gnu/libnl-route-3.so.200.26.0 /usr/lib/x86_64-linux-gnu/libnl-route-3.so
  sudo ln -s /usr/lib/x86_64-linux-gnu/libnl-genl-3.so.200.26.0 /usr/lib/x86_64-linux-gnu/libnl-genl-3.so
else
  sudo ln -s /usr/lib/aarch64-linux-gnu/libnl-genl-3.so.200 /usr/lib/aarch64-linux-gnu/libnl-genl-3.so
  sudo ln -s /usr/lib/aarch64-linux-gnu/libnl-route-3.so.200 /usr/lib/aarch64-linux-gnu/libnl-route-3.so
fi
if uname -a | grep -cs "Ubuntu"> /dev/null 2>&1; then 
  if uname -m | grep -Eq 'x86_64' ; then
    sudo cp $PROJ/material/wfb.conf /etc/NetworkManager/conf.d
    sudo systemctl reload NetworkManager.service 
  fi
fi
#sudo cp $PROJ/material/8812au.conf /etc/modprobe.d
for f in $PROJ/material/60-wfb.rules $PROJ/material/wfb.service $PROJ/scripts/wfb_on.sh; do
  sed -i 's#TOBEUPDATEATINSTALLATION#'$PROJ'#' ${f};
  echo ${f};
done;
sudo cp $PROJ/material/8812au.conf /etc/modprobe.d
sudo cp $PROJ/material/60-wfb.rules /etc/udev/rules.d
sudo cp $PROJ/material/wfb.service /etc/systemd/system
sudo udevadm control --reload-rules
mkdir $PROJ/obj $PROJ/bin
