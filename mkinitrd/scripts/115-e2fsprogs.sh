#!/bin/sh
#
# Description	: The e2fsprogs
# Authors	: jianjun jiang - jjjstudio@gmail.com
# Version	: 0.01
# Notes		: None
# Programs	: 
#
#


# will be compiled with static ?
E2FSPROGS_STATIC=n

# if no enable flag, the script will exit directly.
[ -z $E2FSPROGS_ENABLE ] || [ $E2FSPROGS_ENABLE != "y" ] && { exit 0; }

# check environment config.
[ -z "$E2FSPROGS_CFG" ] && { echo "Error: you must config \$E2FSPROGS_CFG environment var."; exit 1; }

# echo the title.
echo "[e2fsprogs] Create the e2fsprogs...";

# set up the url environment of fmtools.
E2FSPROGS_URL=http://xxx/$E2FSPROGS_CFG.tar.gz;

# source the common function.
source $PROG_DIR/common.sh;

#download unpack and patch the busybox.
GetUnpackAndPatch $E2FSPROGS_URL || { echo "GetUnpackAndPatch fail."; exit 1; }

# enter the source directory.
cd "$BUILD_DIR/$E2FSPROGS_CFG" || { echo "Error: Could not enter the $E2FSPROGS_CFG directory."; exit 1; }

# Create and enter the e2fsprogs build directory.
mkdir -p e2fsprogs_build && cd e2fsprogs_build || { exit 1; }

# make fmtools.
if [ ! -z $E2FSPROGS_STATIC ] && [ $E2FSPROGS_STATIC = "y" ]; then
	echo "not support" && { exit 1; }
else
	CC=$CROSS"gcc" ../configure --prefix=/usr --exec-prefix=/ --sysconfdir=/etc --build=$BUILD --host=$HOST --target=$TARGET || { exit 1; }
	CC=$CROSS"gcc" make || { exit 1; }
	CC=$CROSS"gcc" make DESTDIR=$BUILD_DIR/$E2FSPROGS_CFG/e2fsprogs_root install || { exit 1; }
	cp -a $BUILD_DIR/$E2FSPROGS_CFG/e2fsprogs_root/sbin/{mkfs.ext2,mkfs.ext3,mkfs.ext4} $INITRD_DIR/sbin/ || { exit 1; }
fi

# stripping.
Stripping $INITRD_DIR || { exit 1; }

# successed and exit.
exit 0;
