# Kernel module build command for Octeon II/III targets is:
# make ARCH=mips CROSS_COMPILE=mips64-octeon-linux-
#
# List of object file(s) to be built
obj-m += unifigpio.o

# Uncomment these flags to generate compiler include directory info
# ccflags-y := $(ccflags-y) -xc -E -v

# paths and cross-compilation info etc.
KBUILD_DIR ?= "/root/headers"
CROSS_COMPILE ?= "mips64-octeon-linux-"
ARCH ?= "mips"

# i am pretty sure specifying KERNELDIR, ARCH, and CROSS_COMPILE again on the command line
# is not necessary, but I'm doing it anyway.
all:
	$(MAKE) -C $(KBUILD_DIR) KBUILD_EXTMOD=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

debug:
	$(MAKE) -C $(KBUILD_DIR) KBUILD_EXTMOD=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) CFLAGS_MODULE="-DDEBUG" modules

clean:
	$(MAKE) -C $(KBUILD_DIR) KBUILD_EXTMOD=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

