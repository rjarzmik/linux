#!/bin/sh

. /opt3/env.sh
kernel=`cat /tmp/last_kernel`
out='/tmp/kernel_file.txt'
rm $out 2>&1 >/dev/null
echo -e "set KERNEL $kernel\r\n" >> $out
echo -e "set MTYPE 1257\r\n" >> $out
echo -e "set CMDLINE \"debug rootdelay=6 root=/dev/mmcblk0p2 console=tty0 fbcon=rotate:3 mem=64M psplash=false 2\"\r\n" >> $out
echo -e "boot2\r\n" >> $out

prun "/Storage Card/Linux/haret.exe"
sleep 2
nc 169.254.2.1 9999 < $out

