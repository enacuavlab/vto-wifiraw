# vto-wifiraw

This project provides the wireless interface for ground and board systems.  
Tested on Raspbian, Ubuntu/Debian and Nvidia Jetpack.  

mkdir ~/Projects  
cd ~/Projects  
git clone --recurse-submodules https://github.com/enacuavlab/vto-wifiraw.git  
cd vto-wifiraw
./install.sh  

vto-wifiraw/patched/wfb_on.sh (set air or ground)  

copy vto-wifiraw/wifibroadcast drone.key gs.key  

---------------------------------------------------------------------------------
---------------------------------------------------------------------------------
2023-02-21-raspios-bullseye-arm64-lite, Raspberry PI3: Ground

Debian 11.3 (bulleyes) arm64 on Raspberry PI4,3,0V2: Board and Ground
 
Ubuntu 20.04 (server+desktop) arm64 on Raspberry PI4 : Ground with Paparazzi 

Ubuntu 22.04 (desktop)

Jetpack 4.6 (arm64) on Nvidia Nano and XavierNX:  Board and Ground
