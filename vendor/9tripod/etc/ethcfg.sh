#!/system/busybox/bin/sh

#lqm add for network dhcp
netcfg eth0 up
netcfg eth0 dhcp
