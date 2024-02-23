#!/bin/bash

#Client command to view stream
#/usr/bin/gst-launch-1.0  udpsrc port=5700 ! application/x-rtp, encoding-name=H264,payload=96 ! rtph264depay ! h264parse ! avdec_h264 ! xvimagesink 

if [ $# != 1 ]; then exit; fi
PIDFILE=$1

gst-launch-1.0 nvarguscamerasrc \
  ! 'video/x-raw(memory:NVMM), format=NV12, width=1280, height=720, framerate=30/1' \
  ! nvv4l2h264enc insert-sps-pps=true bitrate=2000000 \
  ! h264parse  \
  ! rtph264pay name=pay0 pt=96 config-interval=1 \
  ! udpsink host=127.0.0.1 port=5700 2>&1 &
echo $! >> $PIDFILE
