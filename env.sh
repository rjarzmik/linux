alias objdump='arm-none-eabi-objdump'
alias ppc='ssh root@192.168.0.202'
alias kmk='make -C /home/rj/mio_linux/kernel SUBDIRS=$PWD'
alias rsync_from_hettar='rsync -avz rj@hettar:${PWD}/ .'
export CROSS_COMPILE=arm-none-eabi-
export PATH=${PATH}:/home/rj/mio_linux/arm-2007q1/bin
export ARCH=arm
. /opt3/env.sh

