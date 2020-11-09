#!/bin/bash -e

# enable wireless
rm -f "${ROOTFS_DIR}/etc/modprobe.d/raspi-blacklist.conf"

# Enable wifi on 5GHz models
mkdir -p "${ROOTFS_DIR}/var/lib/systemd/rfkill/"
echo 0 > "${ROOTFS_DIR}/var/lib/systemd/rfkill/platform-3f300000.mmcnr:wlan"
echo 0 > "${ROOTFS_DIR}/var/lib/systemd/rfkill/platform-fe300000.mmcnr:wlan"

install -m 644 files/hostapd.conf "${ROOTFS_DIR}/etc/hostapd/hostapd.conf"
install -m 644 files/dnsmasq.conf "${ROOTFS_DIR}/etc/dnsmasq.d/wpilib.conf"

cat <<END >> "${ROOTFS_DIR}/etc/wpa_supplicant/wpa_supplicant.conf"

###### BELOW THIS LINE EDITED BY RPICONFIGSERVER ######
network={
    ssid="WPILibPi"
    psk="WPILib2021!"
}
END

cat <<END >> "${ROOTFS_DIR}/etc/dhcpcd.conf"

###### BELOW THIS LINE EDITED BY RPICONFIGSERVER ######


interface wlan0
static ip_address=10.0.0.2/24
static routers=0.0.0.0
nohook wpa_supplicant
END
