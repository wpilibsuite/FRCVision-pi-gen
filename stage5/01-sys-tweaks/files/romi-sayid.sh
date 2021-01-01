#!/bin/bash

[ -f /boot/default-ssid.txt ] || exit 0
[ -f /boot/default-ssid.wav ] || exit 0
[ -f /etc/hostapd/hostapd.conf ] || exit 0

SSID=`grep ^ssid= /etc/hostapd/hostapd.conf | cut -c 6-`
DEFAULTID=`cat /boot/default-ssid.txt 2>/dev/null`

[ "$SSID" = "$DEFAULTID" ] || exit 0

echo $$ > /run/romi_sayid.pid

MESSAGE="Jr ner Ebzv.  Lbh jvyy or nffvzvyngrq."

while true
do
    for i in {1..10}
    do
        aplay -q /boot/default-ssid.wav
    done
    echo $MESSAGE | tr 'A-Za-z' 'N-ZA-Mn-za-m' | espeak -s 140 -g 5 --stdin --stdout | aplay -q
done
