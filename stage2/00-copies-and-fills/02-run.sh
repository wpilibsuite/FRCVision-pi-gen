#!/bin/bash -e

if [ -f "${ROOTFS_DIR}/etc/ld.so.preload" ]; then
   mv "${ROOTFS_DIR}/etc/ld.so.preload" "${ROOTFS_DIR}/etc/ld.so.preload.disabled"
fi

echo "deb https://packages.cloud.google.com/apt coral-edgetpu-stable main" > "${ROOTFS_DIR}/etc/apt/sources.list.d/coral-edgetpu.list"

curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key add -

sudo apt-get update

