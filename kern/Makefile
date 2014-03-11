CPP=i686-pc-elf-g++
CC=i686-pc-elf-gcc
AS=i686-pc-elf-as
LD=i686-pc-elf-ld

LIBCXX_HEADERS=$(dir $(shell which $(CC)))../../libcxx/include/

CURRENT_DIR=$(shell pwd)

ARCH_ARGS=-march=i686 -mtune=pentium3 -msoft-float -mno-sse
WARNINGS=-Werror -Wall -Wformat -Wformat-security -Wformat-nonliteral -Wformat=2 -Wno-multichar -Wno-unused-variable -Wno-unused-function -Wno-deprecated -Wno-format-extra-args
INCLUDES=-I$(CURRENT_DIR) -I. -I$(CURRENT_DIR)/includes/
OPTIONS=-fstack-protector-all -fno-builtin -fno-omit-frame-pointer -falign-functions=4
CPPFLAGS=$(filter-out -std=c99, $(CFLAGS)) -fno-rtti -fno-exceptions -std=gnu++0x

CFLAGS=-pipe -c -g $(ARCH_ARGS) -static -O2 -nostartfiles -nodefaultlibs -ffreestanding -std=c99 $(OPTIONS) $(INCLUDES) $(WARNINGS)
LDFLAGS=-T kern.ld -ffreestanding -O2 -nostdlib -lgcc
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

	$(CPP) $(LDFLAGS) $(OBJECTS) $(addsuffix /*.oa, $(SUBDIRS)) -o $@

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

# Submodules
$(SUBDIRS):
	@echo "\n\n[3;32;40m***** Compiling $@ *****[0;37;49m"
	@$(MAKE) -s -C $@ || exit $$?

$(SUBDIRS_CLEAN):
	@$(MAKE) -C $(subst _clean,, $@) clean

# Cleaning of the shit
clean: $(SUBDIRS_CLEAN)
	@rm -rf $(OBJECTS) $(EXECUTABLE)
