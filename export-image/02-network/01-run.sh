#!/bin/bash -e

on_chroot << EOF
rm -f /etc/resolv.conf

touch /tmp/dhcpcd.resolv.conf
ln -s /tmp/dhcpcd.resolv.conf /etc/resolv.conf

sed -i -e 's/\/run\//\/var\/run\//' /etc/systemd/system/dhcpcd5.service

cp /etc/dhcpcd.conf /boot/
chown -f 0:0 /boot/dhcpcd.conf
ln -sf /boot/dhcpcd.conf /etc/dhcpcd.conf

EOF
