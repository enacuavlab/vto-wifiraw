#!/bin/bash
sysctl -w net.ipv6.conf.all.disable_ipv6=1
./wfb 
