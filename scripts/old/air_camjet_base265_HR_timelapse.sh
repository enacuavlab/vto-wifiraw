#!/bin/bash

#Client command to view stream
#gst-launch-1.0  udpsrc port=5700 ! application/x-rtp, encoding-name=H265,payload=96 ! rtph265depay ! h265parse ! avdec_h265 ! xvimagesink 

if [ $# != 1 ]; then exit; fi
PIDFILE=$1

STORAGEPATH=/home/pprz/tmp
mkdir $STORAGEPATH

gst-launch-1.0 nvarguscamerasrc \
  ! 'video/x-raw(memory:NVMM), format=NV12, width=3264, height=2464, framerate=21/1' \
  ! tee name=streams \
  ! queue \
  ! nvvidconv flip-method=0 \
  ! videorate \
  ! video/x-raw,framerate=1/1 \
  ! nvjpegenc \
  ! multifilesink post-messages=true location="$STORAGEPATH/frame%05d.jpg" streams. \
  ! queue \
  ! nvv4l2h265enc insert-sps-pps=true bitrate=2000000 \
  ! h265parse  \
  ! rtph265pay name=pay0 pt=96 config-interval=1 \
  ! udpsink host=127.0.0.1 port=5700 2>&1 &
echo $! >> $PIDFILE

#gst-launch-1.0 nvarguscamerasrc \
#  ! 'video/x-raw(memory:NVMM), format=NV12, width=3264, height=2464, framerate=21/1' \
#  ! nvv4l2h265enc insert-sps-pps=true bitrate=2000000 \
#  ! h265parse  \
#  ! rtph265pay name=pay0 pt=96 config-interval=1 \
#  ! udpsink host=127.0.0.1 port=5700 2>&1 &
#echo $! >> $PIDFILE
