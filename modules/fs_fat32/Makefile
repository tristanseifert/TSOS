CPP=i686-pc-elf-g++
CC=i686-pc-elf-gcc
AS=i686-pc-elf-as
LD=i686-pc-elf-ld

LIBGCC_PATH=$(dir $(shell which $(CC)))../lib/gcc/i686-pc-elf/4.6.2/libgcc.a

CURRENT_DIR=$(shell pwd)

ARCH_ARGS=-march=i686 -mtune=pentium3 -msoft-float -mno-sse
WARNINGS=-Werror -Wall -Wno-unused-variable -Wno-unused-function -Wno-deprecated -Wformat -Wno-format-extra-args -Wformat-security -Wformat-nonliteral -Wformat=2
INCLUDES=-I$(CURRENT_DIR) -I. -I$(CURRENT_DIR)/../../kern/ -I$(CURRENT_DIR)/../include/
OPTIONS=-fstack-protector-all -ffast-math -fno-builtin -fno-omit-frame-pointer -fvisibility=hidden
CPPFLAGS=$(filter-out -std=c99, $(CFLAGS)) -fno-rtti -fno-exceptions

CFLAGS=-pipe -c -g $(ARCH_ARGS) -static -O2 -nostartfiles -nodefaultlibs -ffreestanding -std=c99 $(OPTIONS) $(INCLUDES) $(WARNINGS)
LDFLAGS=-ffreestanding -g -O2 -nostdlib -Wl,-T,../module.ld -lgcc $(LIBGCC_PATH) -Wl,--unresolved-symbols=ignore-all -Wl,--relocatable
ASFLAGS =

# Export compilers and flags
export CC
export AS
export LD
export CPP
export CFLAGS
export ASFLAGS
export CPPFLAGS

# Use pretty make
export MAKE=$(shell pwd)/pretty_make.py

# Source files to compile
SOURCES=module.c
OBJECTS=$(sort $(filter-out %.c %.s,$(SOURCES:.c=.o) $(SOURCES:.s=.o)))

# Name of final executable
EXECUTABLE=fs_fat32.kmod

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	@i686-pc-elf-objcopy --strip-debug $@

.c.o:
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $< -o $@

.cpp.o:
	@echo "[G++] $<"
	@$(CPP) $(CFLAGS) $(CPPFLAGS) $< -o $@

.s.o:
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@


# Cleaning of the shit
clean:
	@rm -rf $(OBJECTS) $(EXECUTABLE)
