FROM ubuntu:22.04

RUN apt update && apt install -y \
    grub-common \
    grub-pc-bin \
    grub-efi-amd64-bin \
    xorriso \
    mtools \
    imagemagick \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work