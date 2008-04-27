#!/bin/bash

cd /lib/modules/2.6.25-mioa701/kernel
scp -r * root@192.168.0.202:/lib/modules/2.6.25-mioa701/kernel/
#cd /lib/modules/2.6.25-mioa701
#scp -r modules* root@192.168.0.202:/lib/modules/2.6.25-mioa701/

