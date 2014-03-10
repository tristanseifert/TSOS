# Binaries
CPP=i686-pc-elf-g++
CC=i686-pc-elf-gcc
AS=i686-pc-elf-as
LD=i686-pc-elf-ld

CURRENT_DIR=$(shell pwd)

# Name of this kernel module
MODULE_NAME=fs_fat32

# General flags
ARCH_ARGS=-march=i686 -mtune=pentium3 -msoft-float -mno-sse
WARNINGS=-Werror -Wall -Wno-unused-variable -Wno-unused-function -Wno-deprecated -Wformat -Wno-format-extra-args -Wformat-security -Wformat-nonliteral -Wformat=2
INCLUDES=-I$(CURRENT_DIR) -I. -I$(CURRENT_DIR)/../../kern/ -I$(CURRENT_DIR)/../include/
OPTIONS=-fstack-protector-all -fno-builtin -fno-omit-frame-pointer -fvisibility=hidden -DMODULE_NAME=\"$(MODULE_NAME)\"

# C and C++ specifics
CFLAGS=-pipe -c -g $(ARCH_ARGS) -static -O2 -nostartfiles -nodefaultlibs -ffreestanding -std=c99 $(OPTIONS) $(INCLUDES) $(WARNINGS)
CPPFLAGS=$(filter-out -std=c99, $(CFLAGS)) -fno-rtti -fno-exceptions

# Linker and assembler flags
LDFLAGS=-ffreestanding -g -O2 -nostdlib -Wl,-T,../module.ld -lgcc -Wl,--unresolved-symbols=ignore-all -Wl,--relocatable
ASFLAGS =

# Source files to compile
SOURCES=module.cpp info.c fat32.cpp
OBJECTS=$(sort $(filter-out %.c %.s %.cpp,$(SOURCES:.c=.o) $(SOURCES:.s=.o) $(SOURCES:.cpp=.o)))

# Name of final executable
EXECUTABLE=$(MODULE_NAME).kmod

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CPP) $(LDFLAGS) $(OBJECTS) -o $@

	@i686-pc-elf-objdump -b elf32-i386 -d $@ > $@_symbols.txt
	@i686-pc-elf-objcopy --strip-debug $@

.c.o:
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $< -o $@

.cpp.o:
	@echo "[G++] $<"
	@$(CPP) $(CPPFLAGS) $< -o $@

.s.o:
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@


clean:
	@rm -rf $(OBJECTS) $(EXECUTABLE)
