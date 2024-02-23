#!/bin/bash

DEVICES=/proc/net/rtl88XXau
FILES=/tmp/wfb_*.pid

WLS=()

if [ $# == 1 ]; then

  # Call from wfb_on.sh

  subdirs=`ls  -d $DEVICES/*/ 2> /dev/null`
  for i in $subdirs;do
    wl=`basename $i`
    if [[ $(ifconfig | grep -c $wl) != 0 ]]; then WLS+=($wl); fi
  done
  
  subfiles=`ls $FILES 2> /dev/null`
  for pidfile in $subfiles;do
    toberemove=true
    for wl in ${WLS[@]};do
      if [[ "$pidfile" == "/tmp/wfb_"*"$wl".pid ]]; then
        toberemove=false 
        break;
      fi
    done
    if [ $toberemove == true ]; then
      kill `cat $pidfile` > /dev/null 2>&1
      rm $pidfile
    fi
  done

else 

  # Call from systemctl stop

  subdirs=`ls  -d $DEVICES/*/ 2> /dev/null`
  for i in $subdirs;do
    wl=`basename $i`
    if [[ $(ifconfig | grep -c $wl) != 0 ]]; then WLS+=($wl); fi
  done

  if [[ ! -z "$WLS" ]]; then
    for wl in ${WLS[@]};do
      ifconfig $wl down
    done
  fi
  
  subfiles=`ls $FILES 2> /dev/null`
  for pidfile in $subfiles;do
    rm $pidfile
  done 

fi
