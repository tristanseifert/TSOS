#!/bin/sh

# Pretty make!
alias make=/Users/tristanseifert/TSOS/kern/pretty_make.py

# Compile modules
make -C modules/fs_*/ all
make -C modules/vid_*/ all

# If build succeeded, continue
OUT=$?
if [ $OUT -eq 0 ];then
	echo ""
else
	exit
fi

# Create ramdisk
echo "\n[3;32;40m***** Building ramdisk *****[0;37;49m"
./build_ramdisk.sh

# Copy kernel, etc
echo "\n[3;32;40m***** Copying files *****[0;37;49m"

# Mount the disk image
hdiutil attach ./hdd.img

rm -f /Volumes/TSOS/boot/kernel.elf
cp kern/kernel.elf /Volumes/TSOS/boot/kernel.elf
rm -f /Volumes/TSOS/boot/initrd.gz
cp initrd.gz /Volumes/TSOS/boot/initrd.gz

# Copy modules
cp modules/*/*.kmod /Volumes/TSOS/etc/modules/
cp modules/modules.cfg /Volumes/TSOS/etc/modules/modules.cfg

# Clean up OS X's crap
rm -rf /Volumes/TSOS/.fseventsd
rm -rf /Volumes/TSOS/.Trashes

# Remove "dot underbar" resource forks
dot_clean -m /Volumes/TSOS/

# Unmount the disk image
hdiutil detach /Volumes/TSOS/

# Run QEMU
if [ "$1" == "qemu" ]; then
	echo "\n[3;32;40m***** Running QEMU *****[0;37;49m"
	qemu-system-i386 -hda hdd.img -m 256M -soundhw sb16 -net nic,model=e1000 -net user -cpu pentium3 -rtc base=utc -monitor stdio -s
elif [ "$1" == "bochs" ]; then
	echo "\n[3;32;40m***** Running Bochs *****[0;37;49m"
	bochs -f bochsrc.txt -q
else 
	echo "\n[3;32;40m***** Running QEMU *****[0;37;49m"
	qemu-system-i386 -hda hdd.img -m 256M -soundhw sb16 -net nic,model=e1000 -net user -cpu pentium3 -rtc base=utc -monitor stdio -s
fi