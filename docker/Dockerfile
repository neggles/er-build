FROM debian:stretch-slim AS base

ARG DEBIAN_FRONTEND=noninteractive
ARG TOOLCHAIN_SRC=toolchain-build-54.tar.gz

RUN apt-get update && \
	apt-get install -q -y \
	autoconf \
	automake \
	bc \
	bison \
	build-essential \
	curl \
	dpkg-dev \
	fakeroot \
	file \
	flex \
	gawk \
	git \
	libgmp-dev \
	libmpc-dev \
	libmpfr-dev \
	libtool \
	texinfo \
	cpio \
	wget \
    nano \
	pigz \
	jq && \
	rm -rf /var/lib/apt/lists/*

ENV PATH="/opt/cross/bin:${PATH}"


# build the actual toolchain
FROM base AS toolchain-builder

ENV BUILD_ROOT="/usr/src/build"
ENV SRC_ROOT="/usr/src/sources"

WORKDIR ${SRC_ROOT}
ADD ${TOOLCHAIN_SRC} ${SRC_ROOT}

RUN mkdir -p $BUILD_ROOT/binutils $BUILD_ROOT/gcc

RUN mv toolchain/gits/binutils toolchain/gits/gcc toolchain/src/gmp toolchain/src/mpc toolchain/src/mpfr .

WORKDIR ${SRC_ROOT}/gcc
RUN ln -s ../gmp && ln -s ../isl && ln -s ../mpc && ln -s ../mpfr

WORKDIR ${BUILD_ROOT}/binutils
RUN $SRC_ROOT/binutils/configure --prefix=/opt/cross --target=mips64-octeon-linux --disable-multilib --disable-werror
RUN make -j$(nproc) && make install

WORKDIR ${BUILD_ROOT}/gcc
RUN MAKEINFO=missing $SRC_ROOT/gcc/configure --prefix=/opt/cross --target=mips64-octeon-linux --disable-fixed-point --disable-multilib --disable-sim --enable-languages=c --with-abi=64 --with-float=soft --with-mips-plt
RUN make -j$(nproc) all-gcc && make install-gcc


# now make our build environment image
FROM base AS builder

# env vars for build
ENV ER_MODEL=e1000
ENV CROSS_COMPILE="mips64-octeon-linux-"
ENV ARCH="mips"
ENV KERNELDIR=/root/kernel
ENV SHELL=/bin/bash

WORKDIR /root

# we grab the toolchain from toolchain-builder
COPY --from=toolchain-builder /opt/cross /opt/cross

# add and extract the kernel package
ADD ./kernel_*.tar.* /root/

# add bashrc
COPY bash.rc /root/.bashrc

# prep this for interactive use.
# use docker-compose to pull this one out if you want a whole kernel
CMD ["bash"]


# now we build the header tarball
FROM builder AS headers-builder

# env vars for build
ENV ER_MODEL=e1000
ENV CROSS_COMPILE="mips64-octeon-linux-"
ENV ARCH="mips"
ENV KERNELDIR=/root/headers

# go here
WORKDIR /root/kernel

RUN mkdir -p /root/headers

# do initial config gen
RUN make ARCH=$ARCH "ubnt_er_${ER_MODEL}_defconfig"

# do initial module prep build
RUN make -j$(nproc) ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE prepare modules_prepare
RUN make -j$(nproc) ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE modules
RUN cp Module.symvers .config /root/headers
RUN make mrproper

# do headers and scripts
RUN make -j$(nproc) ARCH=$ARCH O=/root/headers CROSS_COMPILE=$CROSS_COMPILE prepare modules_prepare scripts
RUN rm /root/headers/source /root/headers/Makefile

# gather ye headers
RUN find . -path './include/*' -prune -o -path './scripts/*' -prune -o -type f \( -name 'Makefile*' -o -name 'Kconfig*' -o -name 'Kbuild*' -o -name '*.sh' -o -name '*.pl' -o -name '*.lds' -o -name 'Platform' \) -print | cpio -pdm "/root/headers"
RUN cp -a scripts include "/root/headers"
RUN find $(find arch -name include -type d -print) -type f | cpio -pdm "/root/headers"
RUN cp /root/headers/include/config/kernel.release /root/ && printenv >> /root/environment.headers-build


# okay now we have our headers and our toolchain, lets make our module builder
FROM base AS kmod-builder

# env vars for build
ENV ER_MODEL=e1000
ENV CROSS_COMPILE="mips64-octeon-linux-"
ENV ARCH="mips"
ENV KERNELDIR=/root/headers
ENV SHELL=/bin/bash

# get toolchain and headers from our previous stages
COPY --from=toolchain-builder /opt/cross /opt/cross
COPY --from=headers-builder /root/headers /root/headers

WORKDIR /root

# add bashrc
COPY bash.rc /root/.bashrc

# prep this for interactive use.

CMD ["bash"]
