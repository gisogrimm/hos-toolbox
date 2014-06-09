#!/bin/bash
vncserver -alwaysshared -geometry 1024x480 :2
sleep 2
DISPLAY=:2 hos_rtmdisplay
vncserver -kill :2
