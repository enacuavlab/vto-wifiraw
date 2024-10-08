#!/bin/bash

if [ $# != 1 ]; then exit; fi
PIDFILE=$1

#------------------------------------------------------------------------------
# ON BOARD
#--------------------------------------------------
# RADXA ZERO 3W
#--------------------------------
# OPERATIONAL
gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, width=1920, height=1080, framerate=30/1, format='NV12' ! mpph265enc rc-mode=vbr bps=3000000 bps-max=3172000  ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5600 host=127.0.0.1 2>&1 &

gst-launch-1.0 v4l2src device=/dev/video9 ! video/x-raw, width=1280, height=740, framerate=10/1 ! videoscale ! video/x-raw, width=1280, height=736, framerate=10/1 ! mpph265enc rc-mode=vbr bps=3000000 bps-max=3172000 ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400 ! udpsink port=5700 host=127.0.0.1 2>&1 &

#--------------------------------------------------
# NVIDIA
#--------------------------------
# OPERATIONAL
#
#gst-launch-1.0 nvarguscamerasrc   ! 'video/x-raw(memory:NVMM), format=NV12, width=1280, height=720, framerate=30/1'   ! nvv4l2h265enc insert-sps-pps=true bitrate=1000000   ! h265parse    ! rtph265pay name=pay0 pt=96 config-interval=1 mtu=1400   ! udpsink host=127.0.0.1 port=5600 2>&1 &
#echo $! >> $PIDFILE
#gst-launch-1.0 nvarguscamerasrc   ! 'video/x-raw(memory:NVMM), format=NV12, width=1280, height=720, framerate=30/1'   ! nvv4l2h264enc insert-sps-pps=true bitrate=1000000   ! h264parse    ! rtph264pay name=pay0 pt=96 config-interval=1 mtu=1400  ! udpsink host=127.0.0.1 port=5600 2>&1 &
#echo $! >> $PIDFILE

#--------------------------------------------------
# PI
#--------------------------------
# OPERATIONAL
#
#gst-launch-1.0 libcamerasrc ! video/x-raw,width=1280,height=720,framerate=30/1,format=NV12,interlace-mode=progressive,colorimetry=bt709 ! timeoverlay ! v4l2h264enc extra-controls="controls,video_bitrate=4000000" ! video/x-h264,level="(string)4" ! rtph264pay mtu=1400 config-interval=-1 ! udpsink port=5600 host=127.0.0.1  2>&1 &
#echo $! >> $PIDFILE

#--------------------------------
# TEST
#
#gst-launch-1.0 videotestsrc ! video/x-raw,width=1280,height=720,framerate=30/1  ! timeoverlay !  v4l2h264enc extra-controls="controls,video_bitrate=4000000"  !  video/x-h264,level="(string)4" ! rtph264pay mtu=1400 config-interval=-1 ! udpsink port=5600 host=127.0.0.1 2>&1 &
#echo $! >> $PIDFILE



#------------------------------------------------------------------------------
# ON GROUND

#gst-launch-1.0 udpsrc port=5600 ! application/x-rtp, encoding-name=H264, payload=96 ! rtph264depay ! h264parse ! queue ! avdec_h264 !  videoconvert ! autovideosink sync=false  2>&1 &
#echo $! >> $PIDFILE
#gst-launch-1.0 udpsrc port=5600 ! application/x-rtp, encoding-name=H265, payload=96 ! rtph265depay ! h265parse ! queue ! avdec_h265 !  videoconvert ! autovideosink sync=false  2>&1 &
#echo $! >> $PIDFILE


# ---------------------------
#gst-launch-1.0 videotestsrc ! video/x-raw,width=1280,height=720,framerate=30/1,format=NV12,interlace-mode=progressive,colorimetry=bt709 ! timeoverlay ! v4l2h264enc extra-controls="controls,video_bitrate_mode=0,h264_minimum_qp_value=35,h264_maximum_qp_value=35,h264_i_frame_period=30,h264_profile=0,h264_level=11,video_bitrate=1500;" ! video/x-h264,level="(string)4" ! rtph264pay mtu=1400 config-interval=-1 ! udpsink port=5600 host=127.0.0.1

#gst-launch-1.0 videotestsrc ! video/x-raw,width=1280,height=720,framerate=30/1  ! timeoverlay !  v4l2h264enc extra-controls="controls,video_bitrate=4000000"  !  video/x-h264,level="(string)4" ! rtph264pay mtu=1400 config-interval=-1 ! udpsink port=5600 host=127.0.0.1


#gst-launch-1.0 libcamerasrc ! video/x-raw,width=1280,height=720,framerate=30/1,format=NV12,interlace-mode=progressive,colorimetry=bt709 ! timeoverlay ! v4l2h264enc extra-controls="controls,video_bitrate_mode=0,h264_minimum_qp_value=35,h264_maximum_qp_value=35,h264_i_frame_period=30,h264_profile=0,h264_level=11,video_bitrate=1500;" ! video/x-h264,level="(string)4" ! rtph264pay mtu=1400 config-interval=-1 ! udpsink port=5600 host=127.0.0.1

#gst-launch-1.0 libcamerasrc ! video/x-raw,width=1280,height=720,framerate=30/1,format=NV12,interlace-mode=progressive,colorimetry=bt709 ! timeoverlay ! v4l2h264enc ! video/x-h264,level="(string)4" ! rtph264pay mtu=1400 config-interval=-1 ! udpsink port=5600 host=127.0.0.1

#gst-launch-1.0 videotestsrc ! video/x-raw,width=1280,height=720,framerate=30/1  ! timeoverlay !  x264enc bitrate=2000   ! rtph264pay mtu=1400 config-interval=-1 ! udpsink port=5600 host=127.0.0.1
