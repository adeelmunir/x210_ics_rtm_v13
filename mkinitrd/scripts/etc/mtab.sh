#!/bin/sh
#
# Description	: mtab
# Authors	: jianjun jiang - jjjstudio@gmail.com
# Version	: 0.01
# Path		: /etc/usplash.conf
# Notes		: 


# echo the title.
echo "[etc] Create /etc/mtab ...";

# The contents of /etc/mtab file.
cat > $INITRD_DIR/etc/mtab << "EOF"
EOF

# change the owner and permission.
chmod 644 $INITRD_DIR/etc/mtab;
chown 0:0 $INITRD_DIR/etc/mtab;

