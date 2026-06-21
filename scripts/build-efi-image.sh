#!/bin/bash

mkdir -p iso_root/EFI/BOOT

cp EFI/BOOT/BOOTX64.EFI \
   iso_root/EFI/BOOT/

cp boot/grub/grub.cfg \
   iso_root/boot/grub/

echo "Build complete"