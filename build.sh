#!/bin/sh

# Pretty make!
alias make=/Users/tristanseifert/TSOS/kern/pretty_make.py

# Compile
make -B -C kern/ all

# If build succeeded, continue
OUT=$?
if [ $OUT -eq 0 ];then
	echo ""
else
	exit
fi

# Copy kernel, etc
rm -f /Volumes/TSOS/kernel.elf
cp kern/kernel.elf /Volumes/TSOS/kernel.elf

# Clean up OS X's crap
rm -rf /Volumes/TSOS/.fseventsd
rm -rf /Volumes/TSOS/.Trashes

# Remove "dot underbar" resource forks
dot_clean -m /Volumes/TSOS/

# Determine which emulator to run
if [ "$1" == "qemu" ]; then
	echo "\n[3;32;40m***** Running QEMU *****[0;37;49m"
	qemu-system-i386 -hda hdd.img -m 256M -soundhw sb16 -net nic,model=e1000 -net user -cpu pentium3 -monitor stdio -s
elif [ "$1" == "bochs" ]; then
	echo "\n[3;32;40m***** Running Bochs *****[0;37;49m"
	bochs -f bochsrc.txt -q
fi
