# obj-m specifies that this is a kernel module
obj-m += unifigpio.o

# Use these options only to determine compiler include directories.
ccflags-y := $(ccflags-y) -xc -E -v

# paths and cross-compilation info etc.
KERNELDIR ?= /root/headers
CROSS_COMPILE ?= mips64-octeon-linux-
ARCH ?= mips

# i am pretty sure specifying KERNELDIR, ARCH, and CROSS_COMPILE again on the command line
# is not necessary, but I'm doing it anyway.
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) KERNELDIR=$(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) KERNELDIR=$(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

