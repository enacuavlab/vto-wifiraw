#!/bin/bash
STR="-------------------------------------------------------------------------\n"
STR+="This script will remove the communication system \n"
STR+="It must be run on ground and board sides \n\n"
STR+="-------------------------------------------------------------------------\n"
STR+="Do you want to proceed the uninstall " 
echo -e $STR
USER=`basename $PWD`
PROJ=$PWD
read -p "for this side in $PROJ (y/n) ?" ANSWER
if [ ! $ANSWER = "y" ] || [ -z $ANSWER ]; then exit -1; fi
if ! groups | grep -q 'sudo'; then exit -1; fi
#cd $PROJ/rtl8812au
#git apply -R ../material/rtl8812au_v5.6.4.2.patch
DKMS=false
if uname -a | grep -cs "Ubuntu"> /dev/null 2>&1;then DKMS=true; fi
if uname -a | grep -cs "4.9.253-tegra"> /dev/null 2>&1;then DKMS=true; fi
if uname -a | grep -cs "5.10.160-legacy-rk35xx"> /dev/null 2>&1;then DKMS=true; fi
if $DKMS; then
  sudo sed -i  '/blacklist rtl8812au/d' /etc/modprobe.d/blacklist.conf > /dev/null 2>&1  
  drivername=`dkms status | grep 8812 | awk '{print substr($1,1,length($1)-1)}'` 
  driverversion=`dkms status | grep 8812 | awk '{print substr($2,1,length($2)-1)}'` 
  driver=$drivername'/'$driverversion
  sudo dkms uninstall $driver
  sudo dkms remove $drivername --all
else 
  sudo make uninstall
  sudo make clean
fi  
if uname -a | grep -cs "Ubuntu"> /dev/null 2>&1; then 
  sudo rm /etc/NetworkManager/conf.d/wfb.conf
  sudo systemctl reload NetworkManager.service 
fi
#sudo rm /etc/modprobe.d/8812au.conf
sudo systemctl stop wfb.service
sudo systemctl disable wfb.service
sudo rm /etc/systemd/system/wfb.service
sudo rm /etc/modprobe.d/8812au.conf
sudo rm /etc/udev/rules.d/60-wfb.rules
sudo udevadm control --reload-rules
