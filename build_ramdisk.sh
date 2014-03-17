#!/bin/sh

cp modules/fs_fat/fs_fat.kmod ramdisk/
dot_clean -m ramdisk/
./tool/mkramdisk ramdisk
