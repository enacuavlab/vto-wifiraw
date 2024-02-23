#!/bin/bash

PIDFILE=/tmp/wfb.pid

#WIDTH=640
#HEIGHT=480
WIDTH=1280
HEIGHT=720
FPS=30
#BITRATE_VIDEO1=400000
#BITRATE_VIDEO2=400000
BITRATE_VIDEO1=3000000
BITRATE_VIDEO2=3000000

#if uname -a | grep -cs "aarch64"> /dev/null 2>&1; then
  gst-launch-1.0 libcamerasrc \
    ! video/x-raw,width=$WIDTH,height=$HEIGHT,format=NV12,colorimetry=bt601,framerate=$FPS/1,interlace-mode=progressive \
    ! v4l2h264enc extra-controls=controls,repeat_sequence_header=1,video_bitrate=$BITRATE_VIDEO1 \
    ! 'video/x-h264,level=(string)4' \
    ! rtph264pay name=pay0 pt=96 config-interval=1 \
    ! udpsink host=127.0.0.1 port=5700 &
  echo $! >> $PIDFILE
#fi

#rm /tmp/camera*
#/home/pi/Projects/RaspiCV/build/raspicv -t 0 -w $WIDTH -h $HEIGHT -fps $FPS/1 -b $BITRATE_VIDEO1 -g $FPS -vf -hf -cd H264 -n -a ENAC -ae 22 -x /dev/null -r /dev/null -rf gray -o - \
#   | gst-launch-1.0 fdsrc \
#    ! h264parse \
#    ! video/x-h264,stream-format=byte-stream,alignment=au \
#    ! rtph264pay name=pay0 pt=96 config-interval=1 \
#    ! udpsink host=127.0.0.1 port=5600 &
#echo $! >> $PIDFILE
#sleep 3
#gst-launch-1.0 shmsrc socket-path=/tmp/camera3 do-timestamp=true \
#  ! video/x-raw, format=BGR, width=$WIDTH, height=$HEIGHT, framerate=$FPS/1, colorimetry=1:1:5:1  \
#  ! v4l2h264enc extra-controls="controls,video_bitrate=$BITRATE_VIDEO2" \
#  ! rtph264pay name=pay0 pt=96 config-interval=1 \
#  ! udpsink host=127.0.0.1 port=5700 &
#echo $! >> $PIDFILE

#v4l2-ctl --device /dev/video0 \
#  --set-fmt-video=width=$WIDTH,height=$HEIGHT,pixelformat=H264 \
#  --set-parm=$FPS \
#  --set-ctrl video_bitrate=$BITRATE_VIDEO1 \
#  --set-ctrl h264_i_frame_period=10 \
#  --set-ctrl repeat_sequence_header=1 \
#  --stream-mmap=4 --stream-to=- \
#  | gst-launch-1.0 fdsrc \
#    ! h264parse \
#    ! video/x-h264,stream-format=byte-stream,alignment=au \
#    ! rtph264pay name=pay0 pt=96 config-interval=-1 \
#    ! udpsink host=127.0.0.1 port=5700  > /dev/null 2>&1 &
#  echo $! >> $PIDFILE

#v4l2-ctl --device /dev/video0 \
#  --set-fmt-video=width=$WIDTH,height=$HEIGHT,pixelformat=H264 \
#  --set-parm=$FPS \
#  --set-ctrl video_bitrate=500000 \
#  --set-ctrl vertical_flip=1 \
#  --set-ctrl h264_i_frame_period=0 \
#  --stream-mmap=3 --stream-to=- \
#  | gst-launch-1.0 fdsrc \
#    ! h264parse \
#    ! tee name=streams \
#    ! queue max-size-buffers=0 max-size-time=0 max-size-bytes=0 \
#    ! udpsink host=127.0.0.1 port=5200  streams. \
#    ! queue max-size-buffers=0 max-size-time=0 max-size-bytes=0 \
#    ! video/x-h264,stream-format=byte-stream,alignment=au \
#    ! rtph264pay name=pay0 pt=96 config-interval=-1 \
#    ! udpsink host=127.0.0.1 port=5600  > /dev/null 2>&1 &
#  echo $! >> $PIDFILE
