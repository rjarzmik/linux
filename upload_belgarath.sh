#!/bin/sh

OutUrl='gatekeeper@belgarath:/var/www/download/mio_a701/current_kernel/'
kdir='/home/rj/mio_linux/kernel'

scp ${kdir}/arch/arm/boot/zImage ${OutUrl}
(cd / && tar czvf /tmp/modules.tgz lib/modules/2.6.21-hh20)
scp /tmp/modules.tgz ${OutUrl}
