#!/bin/bash

/mnt/d/setups/Python35/python.exe D:\\works/esp32/code/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port "COM7" --baud 921600 --before "default_reset" --after "hard_reset" write_flash -z --flash_mode "dio" --flash_freq "40m" --flash_size detect 0x1000 build/bootloader/bootloader.bin 0x10000 build/esp32-mqtt.bin 0x8000 build/partitions_singleapp.bin
/mnt/d/setups/Python35/python.exe D:\\setups/Python35/Scripts/miniterm.py COM7 115200
