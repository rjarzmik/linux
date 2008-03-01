#!/bin/bash

export PATH=${PATH}:/home/rj/tmp/linux_a701/kernel26_xda/arm-2007q1/bin
exec make CROSS_COMPILE=arm-none-eabi- ARCH=arm CONFIG_DEBUG_SECTION_MISMATCH=y
#pcp arch/arm/boot/zImage :/Program\ Files/haret/zImage
