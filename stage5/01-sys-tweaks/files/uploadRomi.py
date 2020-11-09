#!/usr/bin/env python3 -u

# This file uploads to the Romi using a USB cable

import time
import serial
import os
import sys
import getopt
import subprocess

def main(argv):
    try:
        opts, args = getopt.getopt(argv,"hp:f:",["port=","file="])
    except getopt.GetoptError as err:
        print(err)
        print("Example: uploadRomi.py -p /dev/ttyACM0 -f firmware/.pio/build/a-start32U4/firmware.hex")
        sys.exit(1)

    # Set Defaults
    usbport = '/dev/ttyACM0'
    hexfile = '$NVM_BIN/../lib/node_modules/wpilib-ws-robot-romi/firmware/.pio/build/a-star32U4/firmware.hex'

    for opt, arg in opts:
        if opt == "-h":
            print("uploadRomi.py -p <full_port_path> -f <file_path>")
            sys.exit(1)
        if opt in ("-p", "--port"):
            usbport = arg
        if opt in ("-f", "--file"):
            hexfile = arg

    print("Beginning binary upload to Romi ...")

    # baudrate of 1200 resets the Arduino to boot mode for 8 seconds
    brate = 1200
    print("Resetting Romi to boot mode (should see quickly flashing yellow LED")
    conn = {}
    try:
        conn = serial.Serial(port=usbport, baudrate=1200)
    except serial.SerialException as err:
        print(err)
        sys.exit(1)

    if not conn.isOpen():
        print("Problem connecting to port " + usbport)
        sys.exit(1)

    conn.close()
    # Allow Romi to go into boot mode
    sys.stdout.flush()
    time.sleep(1)
    # Upload binary to Romi
    print("Running imaging tool")
    sys.stdout.flush()
    sys.exit(subprocess.call(['avrdude', '-v', '-q', '-patmega32u4', '-cavr109', '-P' + usbport, '-b57600', '-D', '-Uflash:w:' + hexfile + ':i'], stderr=sys.stdout.fileno()))

if __name__ == "__main__":
    main(sys.argv[1:])

