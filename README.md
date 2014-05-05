Eyeo Touchpoints
====

Wireless capsense with the MPR121 on the Intel Galileo from Arduino to Processing via OSC 

Dependencies: 
OSC: OSCdata.h modified for Intel Galileo architecture. 
  
Notes: 
-Wifi network sometimes requires a WPA supplicant and when restarting this is overwritten. 
-Need to test to make sure this does not happen during the installation.
See: http://www.malinov.com/Home/sergey-s-blog/intelgalileo-addingwifi
  
Router and Firewall:
-Windows and McCaffe Firewalls must be off on the remote PC for messages to get through
-Set Router setting to 60 days, and Static IPs in the variables below for the machines.
  
Serial Debugging:
-For installation turn off serial and use OSC to test so the buffers don't fill
  
MPR121 Wiring Diagrames are located here:
Touchpad: https://learn.sparkfun.com/tutorials/mpr121-hookup-guide/capacitive-touch-keypad

Breakout: https://learn.sparkfun.com/tutorials/mpr121-hookup-guide/capacitive-touch-sensor-breakout-board
  
