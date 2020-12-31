#!/bin/bash -e

SUB_STAGE_DIR=${PWD}

# Override resize2fs_once to add unique SSID setting on first boot
install -m 755 files/resize2fs_once	"${ROOTFS_DIR}/etc/init.d/"

# Install romi_sayid.sh and startup script
install -m 755 files/romi-sayid.sh	"${ROOTFS_DIR}/usr/local/frc/bin/"
install -m 755 files/romi_sayid		"${ROOTFS_DIR}/etc/init.d/"

# enable i2c
install -m 644 files/i2c.conf "${ROOTFS_DIR}/etc/modules-load.d/"
sed -i -e "s/^#dtparam=i2c_arm=on/dtparam=i2c_arm=on/" "${ROOTFS_DIR}/boot/config.txt"

# Install upload tool
install -v -d "${ROOTFS_DIR}/usr/src/wpilib-ws-romi/scripts"
install -m 755 files/uploadRomi.py "${ROOTFS_DIR}/usr/src/wpilib-ws-romi/scripts/"
sed -i -e "s,\\\$NVM_BIN,/home/${FIRST_USER_NAME}/.nvm/versions/node/v14.15.0/bin," "${ROOTFS_DIR}/usr/src/wpilib-ws-romi/scripts/uploadRomi.py"

install -m 644 files/romi.json "${ROOTFS_DIR}/boot/"

# Install NVM and Romi package
on_chroot << EOF
export HOME="/home/${FIRST_USER_NAME}"

curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.37.0/install.sh | bash

export NVM_DIR="/home/${FIRST_USER_NAME}/.nvm"
source "/home/${FIRST_USER_NAME}/.nvm/nvm.sh"

nvm install 14.15.0

npm --unsafe-perm --user=1000 --group=1000 install -g @wpilib/wpilib-ws-robot-romi
npm --unsafe-perm --user=1000 --group=1000 install -g i2c-bus
EOF

# Fix NVM/NPM owner/group
chown -R 1000:1000 "${ROOTFS_DIR}/home/${FIRST_USER_NAME}/.nvm"
chown -R 1000:1000 "${ROOTFS_DIR}/home/${FIRST_USER_NAME}/.config"

# Set up Romi service
install -v -d "${ROOTFS_DIR}/service/wpilibws-romi"
install -m 755 files/wpilibws-romi_run "${ROOTFS_DIR}/service/wpilibws-romi/run"
install -v -d "${ROOTFS_DIR}/service/wpilibws-romi/log"
install -m 755 files/wpilibws-romi_log_run "${ROOTFS_DIR}/service/wpilibws-romi/log/run"

on_chroot << EOF
cd /service/wpilibws-romi && rm -f supervise && ln -s /tmp/wpilibws-romi-supervise supervise
cd /service/wpilibws-romi/log && rm -f supervise && ln -s /tmp/wpilibws-romi-log-supervise supervise
cd /etc/service && rm -f wpilibws-romi && ln -s /service/wpilibws-romi .
EOF

# Replace configServer script to add romi flag
install -v -d "${ROOTFS_DIR}/service/configServer"
install -m 755 files/configServer_run "${ROOTFS_DIR}/service/configServer/run"

