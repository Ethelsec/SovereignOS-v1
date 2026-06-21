#!/usr/bin/env bash
set -e

ROOT_DIR="$(pwd)"
BUILD_DIR="$ROOT_DIR/build"
IMG="$BUILD_DIR/efi.img"

echo "[1/5] Cleaning build..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "[2/5] Creating FAT EFI disk..."
dd if=/dev/zero of="$IMG" bs=1M count=300
mkfs.fat -F32 "$IMG"

echo "[3/5] Mounting image..."
MNT=$(mktemp -d)
sudo mount "$IMG" "$MNT"

echo "[4/5] Copying EFI system files..."

sudo mkdir -p "$MNT/EFI/BOOT"
sudo cp -r iso_root/EFI/BOOT/BOOTX64.EFI "$MNT/EFI/BOOT/"

sudo mkdir -p "$MNT/boot"
sudo cp -r boot/* "$MNT/boot/"

sync
sudo umount "$MNT"
rmdir "$MNT"

echo "[5/5] Done: EFI image rebuilt -> $IMG"