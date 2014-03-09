CPP=i686-pc-elf-g++
CC=i686-pc-elf-gcc
AS=i686-pc-elf-as
LD=i686-pc-elf-ld

LIBGCC_PATH=$(dir $(shell which $(CC)))../lib/gcc/i686-pc-elf/4.6.2/libgcc.a

CURRENT_DIR=$(shell pwd)

ARCH_ARGS=-march=i686 -mtune=pentium3 -msoft-float -mno-sse
WARNINGS=-Werror -Wall -Wno-unused-variable -Wno-unused-function -Wno-deprecated -Wformat -Wno-format-extra-args -Wformat-security -Wformat-nonliteral -Wformat=2
INCLUDES=-I$(CURRENT_DIR) -I. -I$(CURRENT_DIR)/includes/
OPTIONS=-fstack-protector-all -fno-builtin -fno-omit-frame-pointer
CPPFLAGS=$(filter-out -std=c99, $(CFLAGS)) -fno-rtti -fno-exceptions

CFLAGS=-pipe -c -g $(ARCH_ARGS) -static -O2 -nostartfiles -nodefaultlibs -ffreestanding -std=c99 $(OPTIONS) $(INCLUDES) $(WARNINGS)
LDFLAGS=-ffreestanding -g -O2 -nostdlib -Wl,-T,kern.ld -lgcc $(LIBGCC_PATH)
ASFLAGS =

# Build number
BUILD_NUMBER_FILE=buildnum.txt
BUILD_NUMBER_LDFLAGS=-Xlinker --defsym -Xlinker BUILD_NUMBER=$$(cat $(BUILD_NUMBER_FILE))
LDFLAGS+= $(BUILD_NUMBER_LDFLAGS)

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

# Subdirectories with makefiles
SUBDIRS=paging console runtime x86_pc driver_support task bus hal drivers
.PHONY: subdirs $(SUBDIRS)

SUBDIRS_CLEAN=$(addsuffix _clean, $(SUBDIRS))

# Source files to compile
SOURCES=entry.s main.c
OBJECTS=$(sort $(filter-out %.c %.s,$(SOURCES:.c=.o) $(SOURCES:.s=.o)))

# Name of final executable
EXECUTABLE=kernel.elf

all: $(SUBDIRS) print $(SOURCES) $(EXECUTABLE)

print:
	@echo "\n\n[3;32;40m***** Compiling kernel core *****[0;37;49m"

$(EXECUTABLE): $(OBJECTS)
	@echo "\n\n[3;32;40m***** Linking kernel *****[0;37;49m"

	@if ! test -f $(BUILD_NUMBER_FILE); then echo 0 > $(BUILD_NUMBER_FILE); fi
	@echo $$(($$(cat $(BUILD_NUMBER_FILE)) + 1)) > $(BUILD_NUMBER_FILE)

	$(CC) $(LDFLAGS) $(OBJECTS) $(addsuffix /*.oa, $(SUBDIRS)) -o $@

	@i686-pc-elf-objdump -b elf32-i386 -d -S $@ > $@_symbols.txt
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

# Submodules
$(SUBDIRS):
	@echo "\n\n[3;32;40m***** Compiling $@ *****[0;37;49m"
	@$(MAKE) -C $@ all

$(SUBDIRS_CLEAN):
	@$(MAKE) -C $(subst _clean,, $@) clean

# Cleaning of the shit
clean: $(SUBDIRS_CLEAN)
	@rm -rf $(OBJECTS) $(EXECUTABLE)
