#!/bin/bash
SSID=Androidxp
PWD=pprzpprz

if [ $# = 2 ];then ssid=$1;pwd=$2;
else ssid=$SSID;pwd=$PWD;fi

echo $ssid
echo $pwd

DEVICE=/sys/class/net/wl*
if ls $DEVICE 1> /dev/null 2>&1;then
  lo=`ls -la $DEVICE | grep -v usb | awk '{print $9}'`
  if [ ! -z $lo ];then
    wl=`basename $lo`
    ph=`iw dev $wl info | grep wiphy | awk '{print "phy"$2}'`
    nb=`rfkill --raw | grep $ph | awk '{print $1}'`
    st=`rfkill --raw | grep $ph | awk '{print $4}'`
    if [ $st == "blocked" ];then `rfkill unblock $nb`;fi
    killall wpa_supplicant > /dev/null 2>&1
    ifconfig $wl up
    if iw dev $wl scan | grep -cs $ssid > /dev/null 2>&1;then
      wpa_supplicant -B -i $wl -c <(wpa_passphrase $ssid $pwd) > /dev/null 2>&1
      while ! iwconfig $wl | grep -cs $ssid > /dev/null 2>&1;do sleep 1;done
      dhclient $wl
      echo "nameserver 8.8.8.8" >> /etc/resolv.conf
      sysctl net.ipv4.ip_forward=1 > /dev/null 2>&1;
      iptables -t nat -A POSTROUTING -o $wl -j MASQUERADE > /dev/null 2>&1;
    fi
  fi
fi
