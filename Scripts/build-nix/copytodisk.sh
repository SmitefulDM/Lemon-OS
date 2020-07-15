#!/bin/bash
if [ -z $1 ]; then
	sudo Scripts/build-nix/mount.sh
else
	sudo mount $1 /mnt/Lemon
fi

sudo cp initrd.tar /mnt/Lemon/lemon/initrd.tar
sudo cp Kernel/build/kernel.sys /mnt/Lemon/lemon/kernel.sys
sudo cp -r Base/* /mnt/Lemon/

if [ -z $1 ]; then
	sudo Scripts/build-nix/unmount.sh
else
	sudo umount /mnt/Lemon
fi
