#!/bin/bash

USBFILE=/tmp/wfb.usb
PIDFILE=/tmp/wfb.pid

if [ -f "$USBFILE" ]; then
  rm $USBFILE  > /dev/null 2>&1
  if [ -f "$PIDFILE" ]; then
    systemctl stop wfb.service
  else
    systemctl start wfb.service
  fi
else
  echo -n >$USBFILE
fi
