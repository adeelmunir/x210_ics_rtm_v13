#!/bin/bash
# create: liuqiming
# date:   2011-12-16
# mail:   phosphor88@163.com

#在终端打印提示信息
echo "Modify the android.img.cpio"
echo "1.unzip the image"
echo "2.Create the image"
echo "3.exit"
#声明环境变量
SOURCE_DIR=$(cd `dirname $0` ; pwd)
TOOLS_DIR=${SOURCE_DIR}/tools
TARGET_DIR=${SOURCE_DIR}/out/release
#读取输入的键值
read -p "Choose:" CHOOSE
#根据输入的键值进行相关操作
if [ "1" = ${CHOOSE} ];then
	echo "unzip android.img.cpio"
	cd ${TARGET_DIR}
	[ -e "tmp" ] ||{ echo "mkdir tmp"; mkdir tmp;}
	[ -e "android.img.cpio" ] || { echo "error!can't find andaroid.img.cpio!"; exit; }
	cd tmp
	#解压cpio文件包
	cpio -idmv --no-absolute-filenames < ../android.img.cpio
	echo "^_^ unzip android.img.cpio finished!"
	exit

elif [ "2" = ${CHOOSE} ];then
	echo "create android.img.cpio"
	[ -e "${TARGET_DIR}/tmp" ] || { echo "can't find [tmp],please unzip android.img.cpio first!"; exit; }
	rm -f ${TARGET_DIR}/cpio_list
	rm -f ${TARGET_DIR}/android.img.cpio
	#打包cpio文件包
	$TOOLS_DIR/gen_initramfs_list.sh ${TARGET_DIR}/tmp > ${TARGET_DIR}/cpio_list || { exit; }
	$TOOLS_DIR/gen_init_cpio ${TARGET_DIR}/cpio_list > ${TARGET_DIR}/android.img.cpio || { exit; }
	rm -rf ${TARGET_DIR}/tmp
	echo "^_^ Create android.img.cpio finished!"
	exit
elif [ "3" = ${CHOOSE} ];then
	exit
fi

