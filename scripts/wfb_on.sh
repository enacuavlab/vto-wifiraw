#!/bin/bash

HOME_PRJ=TOBEUPDATEATINSTALLATION/src

PIDFILE=/tmp/wfb.pid

sysctl -w net.ipv6.conf.all.disable_ipv6=1
$HOME_PRJ/wfb > /dev/null 2>&1 &
echo $! | tee -a $PIDFILE > /dev/null 2>&1 
#$HOME_PRJ/video.sh $PIDFILE > /dev/null 2>&1 &
#echo $! | tee -a $PIDFILE > /dev/null 2>&1 
