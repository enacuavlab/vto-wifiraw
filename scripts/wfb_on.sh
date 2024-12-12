#!/bin/bash

HOME_PRJ=TOBEUPDATEATINSTALLATION

PIDFILE=/tmp/wfb.pid

sysctl -w net.ipv6.conf.all.disable_ipv6=1
$HOME_PRJ/bin/wfb > /dev/null 2>&1 &
echo $! | tee -a $PIDFILE > /dev/null 2>&1 
#$HOME_PRJ/scripts/video.sh $PIDFILE > /dev/null 2>&1 &
#echo $! | tee -a $PIDFILE > /dev/null 2>&1 
