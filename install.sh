#!/bin/bash

. /opt3/sync-engine/env.sh
date=`date +%Y_%m_%d_%Hh%M`
file="zImage_$date"
pcp arch/arm/boot/zImage ":/Storage Card/Linux/$file"
echo "$file" > /tmp/last_kernel
