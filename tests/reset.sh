#!/bin/bash
sudo iw dev wlxfc349725a319 set type managed
sudo ip link set wlxfc349725a319 down
sudo iw dev wlx3c7c3fa9bfbb set type managed
sudo ip link set wlx3c7c3fa9bfbb down
