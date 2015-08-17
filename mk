#!/bin/sh
#
# Description		: Build Android Script.
# Authors		: http://www.9tripod.com
# Version		: 0.01
# Notes			: None
#

export ANDROID_JAVA_HOME=/usr/lib/jvm/java-6-sun/
SOURCE_DIR=$(cd `dirname $0` ; pwd)

TOOLS_DIR=${SOURCE_DIR}/tools
RELEASE_DIR=${SOURCE_DIR}/out/release
TARGET_DIR=${SOURCE_DIR}/out/target/product/x210
KERNEL_DIR=${SOURCE_DIR}/kernel
BOOTLOADER_UBOOT_SD_CONFIG=x210_sd_config
BOOTLOADER_UBOOT_NAND_CONFIG=x210_nand_config
BOOTLOADER_XBOOT_CONFIG=arm32-x210ii
KERNEL_NAND_CONFIG=x210_android_nand_defconfig
KERNEL_INAND_CONFIG=x210_android_inand_defconfig
INITRD_KERNEL_CONFIG=x210_initrd_defconfig
FILESYSTEM_CONFIG=PRODUCT-full_x210-eng

export PATH=$SOURCE_DIR/uboot/tools:$PATH
export PATH=$SOURCE_DIR/out/host/linux-x86/bin:$PATH

CPU_NUM=$(cat /proc/cpuinfo |grep processor|wc -l)
CPU_NUM=$((CPU_NUM+1))

setup_environment()
{
	cd ${SOURCE_DIR};
	mkdir -p ${RELEASE_DIR} || return 1;
	mkdir -p ${TARGET_DIR} || return 1;
	mkdir -p ${TARGET_DIR}/system/app || return 1;
	mkdir -p ${TARGET_DIR}/system/etc || return 1;
}

build_bootloader_uboot_nand()
{
	cd ${SOURCE_DIR}/uboot || return 1
	make distclean || return 1;
	make ${BOOTLOADER_UBOOT_NAND_CONFIG} || return 1;
	make -j${CPU_NUM}
        mv u-boot.bin uboot_nand.bin

        if [ -f uboot_nand.bin ]; then
		cp uboot_nand.bin ${RELEASE_DIR}/uboot.bin
		cd ${RELEASE_DIR}
		${SOURCE_DIR}/tools/mkheader uboot.bin
		echo "^_^ uboot_nand.bin is finished successful!"
		exit
	else
		echo "make error,cann't compile u-boot.bin!"
		exit
	fi

	return 0
}

build_bootloader_uboot_inand()
{
	cd ${SOURCE_DIR}/uboot || return 1
	make distclean || return 1;
	make ${BOOTLOADER_UBOOT_SD_CONFIG} || return 1;
	make -j${CPU_NUM}
        mv u-boot.bin uboot_inand.bin

        if [ -f uboot_inand.bin ]; then
		cp uboot_inand.bin ${RELEASE_DIR}/uboot.bin
		cd ${RELEASE_DIR}
		${SOURCE_DIR}/tools/mkheader uboot.bin
		echo "^_^ uboot_inand.bin is finished successful!"
		exit
	else
		echo "make error,cann't compile u-boot.bin!"
		exit
	fi

	return 0
}

build_kernel_initrd()
{
	cd ${SOURCE_DIR}/kernel || return 1

	make ${INITRD_KERNEL_CONFIG} || return 1
	make -j${threads} || return 1
	dd if=${SOURCE_DIR}/kernel/arch/arm/boot/zImage of=${RELEASE_DIR}/zImage-initrd bs=2048 count=8192 conv=sync;

	echo "" >&2
	echo "^_^ initrd kernel path: ${RELEASE_DIR}/zImage-initrd" >&2

	return 0
}

build_bootloader_xboot()
{
	if [ ! -f ${RELEASE_DIR}/zImage-initrd ]; then
		echo "No zImage-initrd, build kernel now!" >&2
		build_kernel_initrd || exit 1
	fi

	if [ ! -f ${RELEASE_DIR}/zImage-initrd ]; then
		echo "not found kernel zImage-initrd, please build kernel first" >&2
		return 1
	fi

	if [ ! -f ${RELEASE_DIR}/zImage-android ]; then
		echo "not found kernel zImage-android, please build kernel first" >&2
		return 1
	fi

	# copy zImage-initrd and zImage-android to xboot's romdisk directory
	cp -v ${RELEASE_DIR}/zImage-initrd ${SOURCE_DIR}/xboot/src/arch/arm32/mach-x210ii/romdisk/boot || return 1;
	cp -v ${RELEASE_DIR}/zImage-android ${SOURCE_DIR}/xboot/src/arch/arm32/mach-x210ii/romdisk/boot || return 1;

	# compiler xboot
	cd ${SOURCE_DIR}/xboot || return 1
	make TARGET=${BOOTLOADER_XBOOT_CONFIG} CROSS=/usr/local/arm/arm-2012.09/bin/arm-none-eabi- clean || return 1;
	make TARGET=${BOOTLOADER_XBOOT_CONFIG} CROSS=/usr/local/arm/arm-2012.09/bin/arm-none-eabi- || return 1;

	# rm zImage-initrd and zImage-android
	rm -fr ${SOURCE_DIR}/xboot/src/arch/arm32/mach-x210ii/romdisk/boot/zImage-initrd
	rm -fr ${SOURCE_DIR}/xboot/src/arch/arm32/mach-x210ii/romdisk/boot/zImage-android

	# copy xboot.bin to release directory
	cp -v ${SOURCE_DIR}/xboot/output/xboot.bin ${RELEASE_DIR}

	echo "" >&2
	echo "^_^ xboot path: ${RELEASE_DIR}/xboot.bin" >&2
	return 0
}

build_kernel_nand()
{
	cd ${SOURCE_DIR}/kernel || return 1

	make ${KERNEL_NAND_CONFIG} || return 1
	make -j${threads} || return 1
	dd if=${SOURCE_DIR}/kernel/arch/arm/boot/zImage of=${RELEASE_DIR}/zImage-android bs=2048 count=8192 conv=sync;

	echo "" >&2
	echo "^_^ android kernel for nand path: ${RELEASE_DIR}/zImage-android" >&2

	return 0
}

build_kernel_inand()
{
	cd ${SOURCE_DIR}/kernel || return 1

	make ${KERNEL_INAND_CONFIG} || return 1
	make -j${threads} || return 1
	dd if=${SOURCE_DIR}/kernel/arch/arm/boot/zImage of=${RELEASE_DIR}/zImage-android bs=2048 count=8192 conv=sync;

	echo "" >&2
	echo "^_^ android kernel for inand path: ${RELEASE_DIR}/zImage-android" >&2

	return 0
}

build_system()
{
	# install android busybox
	tar xf ./vendor/9tripod/busybox.tgz -C ${TARGET_DIR}/system/
	# install android app
	#cp ./vendor/9tripod/app/*              ${TARGET_DIR}/system/app/ -a
	# install etc
	#cp ./vendor/9tripod/etc/*              ${TARGET_DIR}/system/etc/ -a
	
	cd ${SOURCE_DIR} || return 1
	make -j${threads} ${FILESYSTEM_CONFIG} || return 1

	# create android.img.cpio
	rm -fr ${TARGET_DIR}/cpio_list ${TARGET_DIR}/android.img.cpio || { return 1; }
	$TOOLS_DIR/gen_initramfs_list.sh ${TARGET_DIR}/root > ${TARGET_DIR}/cpio_list || { return 1; }
	$TOOLS_DIR/gen_init_cpio ${TARGET_DIR}/cpio_list > ${TARGET_DIR}/android.img.cpio || { return 1; }

	# create data.tar
	cd ${TARGET_DIR}/data || { echo "Error: Could not enter the ${TARGET_DIR}/data directory."; return 1; }
	rm -fr ${TARGET_DIR}/data.tar || { return 1; }
	tar cvf ${TARGET_DIR}/data.tar ./* || { return 1; }

	# create system.tar
	cd ${TARGET_DIR}/system || { echo "Error: Could not enter the ${TARGET_DIR}/system directory."; return 1; }
	rm -fr ${TARGET_DIR}/system.tar || { return 1; }
	tar cvf ${TARGET_DIR}/system.tar ./* || { return 1; }

	#cp -av ${TARGET_DIR}/installed-files.txt ${RELEASE_DIR}/ || return 1;
	#cp -av ${TARGET_DIR}/installed-files.txt ${RELEASE_DIR}/ || return 1;
	cp -av ${TARGET_DIR}/android.img.cpio ${RELEASE_DIR}/ || return 1;
	cp -av ${TARGET_DIR}/android.img.cpio ${KERNEL_DIR}/ || return 1;
	cp -av ${TARGET_DIR}/system.img ${RELEASE_DIR}/x210.img || return 1;
	cp -av ${TARGET_DIR}/system.tar ${RELEASE_DIR}/ || return 1;
	cp -av ${TARGET_DIR}/userdata.img ${RELEASE_DIR}/ || return 1;
	cp -av ${TARGET_DIR}/data.tar ${RELEASE_DIR}/ || return 1;

	echo "" >&2
	echo "^_^ system path: ${RELEASE_DIR}/system.tar" >&2

	# create uboot_img
	#cd ${TARGET_DIR}
	#echo '****** Make ramdisk image for u-boot ******'
	#mkimage -A arm -O linux -T ramdisk -C none -a 0x30800000 -n "ramdisk" -d ramdisk.img x210-uramdisk.img
	#cp x210-uramdisk.img $RELEASE_DIR/
	#rm -f ramdisk.img
	
	return 0
}

build_yaffs_image()
{
	rm ./x210_root -rf
	mkdir x210_root

	cp ./out/target/product/x210/system/lib/libstagefright*             ./vendor/9tripod/lib/
	cp ./out/target/product/x210/root/* 	./x210_root/ -a
	cp ./out/target/product/x210/system 	./x210_root/ -a
	#cp ./vendor/9tripod/app/*              	./x210_root/system/app/ -a
	cp ./vendor/9tripod/lib/*              	./x210_root/system/lib/ -a
	rm ./x210_root/system/lib/modules/wlan.ko -rf
	cp ./vendor/9tripod/nand/wlan.ko	./x210_root/system/lib/modules/wlan.ko -a
	#cp ./vendor/9tripod/etc/*              	./x210_root/system/etc/ -a
	#cp ./vendor/9tripod/usr/*              	./x210_root/system/usr/ -a
	#cp ./vendor/9tripod/bin/*	       	./x210_root/system/bin/ -a

	rm x210_root/init.rc
	cp device/samsung/x210/init.rc ./x210_root

	chmod 777 ./x210_root/system/vendor/bin/pvrsrvinit
	#chmod 777 ./x210_root/system/bin/bluetoothd
	chmod 777 ./x210_root/system/bin/hciattach 
	#chmod 777 ./x210_root/system/bin/pand
	#chmod 777 ./x210_root/system/bin/sdptool 
	#rm -rf ./x210_root/system/busybox
	#tar zxvf ./vendor/9tripod/busybox.tgz -C ./x210_root/system/

	mkyaffs2image x210_root/ x210.img

	rm ${RELEASE_DIR}/x210.img
	mv x210.img $RELEASE_DIR/
}

gen_update_bin()
{
	# check image files
	if [ ! -f ${RELEASE_DIR}/xboot.bin ]; then
		echo "not found bootloader xboot.bin, please build bootloader" >&2
		return 1
	fi

	if [ ! -f ${RELEASE_DIR}/zImage-initrd ]; then
		echo "not found kernel zImage-initrd, please build kernel first" >&2
		return 1
	fi

	if [ ! -f ${RELEASE_DIR}/zImage-android ]; then
		echo "not found kernel zImage-android, please build kernel first" >&2
		return 1
	fi

	if [ ! -f ${RELEASE_DIR}/system.tar ]; then
		echo "not found system.tar, please build system" >&2
		return 1
	fi

	if [ ! -f ${RELEASE_DIR}/data.tar ]; then
		echo "not found data.tar, please build system" >&2
		return 1
	fi

	rm -fr ${RELEASE_DIR}/tmp || return 1;
	rm -fr ${RELEASE_DIR}/android-update.bin || return 1;
	mkdir -p ${RELEASE_DIR}/tmp || return 1;

	# copy image files
	cp ${RELEASE_DIR}/xboot.bin ${RELEASE_DIR}/tmp/;
	cp ${RELEASE_DIR}/zImage-initrd ${RELEASE_DIR}/tmp/;
	cp ${RELEASE_DIR}/zImage-android ${RELEASE_DIR}/tmp/;
	cp ${RELEASE_DIR}/system.tar ${RELEASE_DIR}/tmp/;
	cp ${RELEASE_DIR}/data.tar ${RELEASE_DIR}/tmp/;

	# create md5sum.txt
	cd ${RELEASE_DIR}/tmp/;
	find . -type f -print | while read line; do
		if [ $line != 0 ]; then
			md5sum ${line} >> md5sum.txt
		fi
	done

	# mkisofs
	mkisofs -l -r -o ${RELEASE_DIR}/android-update.bin ${RELEASE_DIR}/tmp/ || return 1;

	cd ${SOURCE_DIR} || return 1 
	rm -fr ${RELEASE_DIR}/tmp || return 1;
	return 0;
}

threads=4;
uboot_nand=no;
uboot_inand=no;
xboot=no;
kernel_nand=no;
kernel_inand=no;
system=no;
update=no;

if [ -z $1 ]; then
	uboot_nand=no
	uboot_inand=yes
	xboot=yes
	kernel_nand=no
        kernel_inand=yes
	yaffs_system=no
	system=yes
	update=yes
fi

while [ "$1" ]; do
    case "$1" in
	-j=*)
		x=$1
		threads=${x#-j=}
		;;
	-un|--uboot_nand)
		uboot_nand=yes
		uboot_inand=no
	    ;;
	-ui|--uboot_inand)
		uboot_inand=yes
		uboot_nand=no
	    ;;
	-x|--xboot)
		xboot=yes
	    ;;
	-kn|--kernel_nand)
	    kernel_nand=yes
            kernel_inand=no
	    ;;
        -ki|--kernel_inand)
	    kernel_nand=no
	    kernel_inand=yes
	    ;;
	-s|--system)
		system=yes
	    ;;
	-y|--yaffs2)
		yaffs_system=yes
	    ;;
	-U|--update)
		update=yes
	    ;;
	-a|--all)
		uboot_nand=no
		uboot_inand=yes
		xboot=yes
		kernel_nand=no
                kernel_inand=yes
		yaffs_system=no
		system=yes
		update=yes
	    ;;
	-h|--help)
	    cat >&2 <<EOF
Usage: build.sh [OPTION]
Build script for compile the source of telechips project.

  -j=n                 	using n threads when building source project (example: -j=16)
  -ui, --uboot_inand   	build bootloader uboot for sd from source file
  -un, --uboot_nand 	build bootloader uboot for nand from source file
  -x, --xboot          	build bootloader xboot from source file
  -kn, --kernel_nand	build kernel for nand flash android and using default config file
  -ki, --kernel_inand  	build kernel for inand flash android and using default config file
  -s, --system         	build file system from source file
  -y, --yaffs2		build yaffs2 android system
  -U, --update         	gen update package update.bin
  -a, --all            	build all, include anything
  -h, --help           	display this help and exit
EOF
	    exit 0
	    ;;
	*)
	    echo "mk: Unrecognised option $1" >&2
	    exit 1
	    ;;
    esac
    shift
done

setup_environment || exit 1

if [ "${system}" = yes ]; then
	build_system || exit 1
fi

if [ "${yaffs_system}" = yes ]; then
	build_yaffs_image || exit 1
fi

if [ "${kernel_nand}" = yes ]; then
	build_kernel_nand || exit 1
fi

if [ "${kernel_inand}" = yes ]; then
	build_kernel_inand || exit 1
fi

if [ "${uboot_nand}" = yes ]; then
	build_bootloader_uboot_nand || exit 1
fi

if [ "${uboot_inand}" = yes ]; then
	build_bootloader_uboot_inand || exit 1
fi

if [ "${xboot}" = yes ]; then
	build_bootloader_xboot || exit 1
fi

if [ "${update}" = yes ]; then
	gen_update_bin || exit 1
fi

exit 0

