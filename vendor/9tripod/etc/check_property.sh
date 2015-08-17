#!/system/busybox/bin/sh

#check timezone
if [ -e /data/property/persist.sys.timezone ]
then
	echo update timezone
else
	cp /etc/predata/persist.sys.timezone /data/property/
	sync
fi

#check softmac
if [ -e /system/wifi/softmac ]
then
	echo update softmac
else
	touch /system/wifi/softmac
	echo 00:50:`expr $RANDOM % 89 + 10`:`expr $RANDOM % 89 + 10`:`expr $RANDOM % 89 + 10`:`expr $RANDOM % 89 + 10` > /system/wifi/softmac
	sync
fi

#check bdaddr_mac
if [ -e /system/etc/bluetooth/bdaddr_mac ]
then
	echo update bdaddr_mac
else
	touch /system/etc/bluetooth/bdaddr_mac
	echo ae:2d:`expr $RANDOM % 89 + 10`:`expr $RANDOM % 89 + 10`:`expr $RANDOM % 89 + 10`:`expr $RANDOM % 89 + 10` > /system/etc/bluetooth/bdaddr_mac
sync
fi

if [ -e /sys/block/mtdblock4 ]
then
/system/busybox/bin/busybox mount -t ext4 /dev/block/mmcblk0p4 /system/vendor
fi

