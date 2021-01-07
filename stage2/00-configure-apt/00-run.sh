#!/bin/bash -e

echo "adding repo for edgetpu"
echo "deb https://packages.cloud.google.com/apt coral-edgetpu-stable main" > "${ROOTFS_DIR}/etc/apt/sources.list.d/coral-edgetpu.list"

on_chroot apt-key add - < files/edgetpu.gpg
on_chroot << EOF
apt-get update
apt-get dist-upgrade -y
pip3 install https://github.com/google-coral/pycoral/releases/download/release-frogfish/tflite_runtime-2.5.0-cp37-cp37m-linux_armv7l.whl
EOF
