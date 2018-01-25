make mrproper

cp /boot/config-$(shell uname -r) .config

sh -c 'yes "" | make oldconfig'

sudo make -j20 bzImage

sudo make -j20 modules

sudo make -j20 modules_install

sudo make install