#!/bin/bash -e

echo "deb https://packages.cloud.google.com/apt coral-edgetpu-stable main" > "${ROOTFS_DIR}/etc/apt/sources.list.d/coral-edgetpu.list"

curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | on_chroot apt-key add -
on_chroot << EOF
apt-get update
EOF
