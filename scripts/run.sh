#!/usr/bin/env bash

qemu-system-x86_64 \
  -m 2048 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/usr/share/OVMF/OVMF_VARS_4M.fd \
  -drive format=raw,file=build/efi.img \
  -vga std \
  -serial stdio