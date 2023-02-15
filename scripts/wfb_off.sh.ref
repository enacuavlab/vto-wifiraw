#!/bin/bash

#PIDFILE=/tmp/wfb.pid
#
#if [ -f "$PIDFILE" ]; then 
#  kill `cat $PIDFILE` > /dev/null 2>&1
#  rm $PIDFILE
#fi

DEVICES=/proc/net/rtl88XXau
FILES=/tmp/wfb_*.pid
WLS=()

if ls $DEVICES 1> /dev/null 2>&1; then

  if [[ $(uname -a | grep -c "4.9.*tegra" ) == 1 ]]; then TEGRA=true; else TEGRA=false; fi
  
  wls=`ls -d $DEVICES/*/`
  for i in $wls;do
    wl=`basename $i`
    if $TEGRA;then
      ty=`iwconfig $wl | grep -c "Mode:Monitor"`
      if [ $ty == '1' ];then WLS+=($wl);fi
    else
      ty=`iw dev $wl info | grep "type" | awk '{print $2}'`
      if [ $ty == "monitor" ]; then
        WLS+=($wl)
      fi
    fi
  done
fi


if ls $FILES 1> /dev/null 2>&1; then

  for pidfile in $FILES; do 
    toberemove=true
    for wl in ${WLS[@]};do
      if [[ "$pidfile" == "/tmp/wfb_"*"$wl".pid ]]; then
        toberemove=false 
	break
      fi
    done
    if [ $toberemove == true ]; then
      kill `cat $pidfile` > /dev/null 2>&1
      rm $pidfile
    fi
  done
fi
