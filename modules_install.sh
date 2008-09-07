#!/bin/bash

rm -rf /tmp/kernel_mods_root
make modules_install INSTALL_MOD_PATH=/tmp/kernel_mods_root

cd /tmp/kernel_mods_root && tar c . | ssh root@mio "cd / && tar xv"
cd /lib/modules/2.6.25-mioa701/kernel

