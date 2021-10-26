tar -xf headers.tar
cd module/src
make -j$(nproc) ARCH=mips CROSS_COMPILE=mips64-octeon-linux- KERNELDIR=/root/headers -C $KERNELDIR M=$(pwd) modules
mips64-octeon-linux-strip --strip-debug wireguard.ko

docker run -v "$(pwd)/unifigpio:/root/module" -it neg2led/er-i-modbuilder bash

make -j$(nproc) -C $KERNELDIR M=$(pwd) EXTRA_CFLAGS='-save-temps' modules


(cd ../headers && mips64-octeon-linux-gcc -Wp,-MD,/root/module/.unifigpio.o.d  -nostdinc -isystem /opt/cross/lib/gcc/mips64-octeon-linux/4.7.0/include -I./arch/mips/include -I./arch/mips/include/generated/uapi -I./arch/mips/include/generated  -I./include -I./arch/mips/include/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/kconfig.h -D__KERNEL__ -DVMLINUX_LOAD_ADDRESS=0xffffffff80800000 -DDATAOFFSET=0 -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -std=gnu89 -fno-PIE -mno-check-zero-division -mabi=64 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -fno-asynchronous-unwind-tables -DGAS_HAS_SET_HARDFLOAT -Wa,-msoft-float -ffreestanding -march=octeon2 -Wa,--trap -DCVMX_BUILD_FOR_LINUX_KERNEL=1 -DUSE_RUNTIME_MODEL_CHECKS=1 -DCVMX_ENABLE_PARAMETER_CHECKING=0 -DCVMX_ENABLE_CSR_ADDRESS_CHECKING=0 -DCVMX_ENABLE_POW_CHECKS=0 -I./arch/mips/include/asm/mach-cavium-octeon -I./arch/mips/include/asm/mach-generic -msym32 -DKBUILD_64BIT_SYM32 -fno-delete-null-pointer-checks -O2 -Wno-maybe-uninitialized --param=allow-store-data-races=0 -DCC_HAVE_ASM_GOTO -Wframe-larger-than=2048 -fno-stack-protector -Wno-unused-but-set-variable -fomit-frame-pointer -fno-var-tracking-assignments -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fno-stack-check -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -save-temps  -DMODULE -mlong-calls  -DKBUILD_BASENAME='"unifigpio"'  -DKBUILD_MODNAME='"unifigpio"' -E -c -o /root/module/unifigpio.o.c /root/module/unifigpio.c)
