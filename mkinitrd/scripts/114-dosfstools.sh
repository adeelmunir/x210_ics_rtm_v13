#!/bin/sh
#
# Description	: The dosfstools
# Authors	: jianjun jiang - jjjstudio@gmail.com
# Version	: 0.01
# Notes		: None
# Programs	: 
#
#


# will be compiled with static ?
DOSFSTOOLS_STATIC=n

# if no enable flag, the script will exit directly.
[ -z $DOSFSTOOLS_ENABLE ] || [ $DOSFSTOOLS_ENABLE != "y" ] && { exit 0; }

# check environment config.
[ -z "$DOSFSTOOLS_CFG" ] && { echo "Error: you must config \$DOSFSTOOLS_CFG environment var."; exit 1; }

# echo the title.
echo "[dosfstools] Create the dosfstools...";

# set up the url environment of fmtools.
DOSFSTOOLS_URL=http://www.daniel-baumann.ch/software/dosfstools/$DOSFSTOOLS_CFG.tar.bz2;

# source the common function.
source $PROG_DIR/common.sh;

#download unpack and patch the busybox.
GetUnpackAndPatch $DOSFSTOOLS_URL || { echo "GetUnpackAndPatch fail."; exit 1; }

# enter the source directory.
cd "$BUILD_DIR/$DOSFSTOOLS_CFG" || { echo "Error: Could not enter the $DOSFSTOOLS_CFG directory."; exit 1; }

# make fmtools.
if [ ! -z $DOSFSTOOLS_STATIC ] && [ $DOSFSTOOLS_STATIC = "y" ]; then
	echo "not support" && { exit 1; }
else
	sed -ie 's/PREFIX = \/usr\/local/PREFIX ?= \/usr\/local/' $BUILD_DIR/$DOSFSTOOLS_CFG/Makefile || { exit 1; }
	CC=$CROSS"gcc" make || { exit 1; }
	CC=$CROSS"gcc" PREFIX=/$BUILD_DIR/$DOSFSTOOLS_CFG/dosfstools_root/usr/ make install || { exit 1; }
	cp -a $BUILD_DIR/$DOSFSTOOLS_CFG/dosfstools_root/usr/sbin/* $INITRD_DIR/usr/sbin/
fi

# stripping.
Stripping $INITRD_DIR || { exit 1; }

# successed and exit.
exit 0;
