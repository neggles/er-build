#!/bin/bash

# Set up environment
export ARCH
export CROSS_COMPILE
export KERNELDIR

alias make-module='make -j$(nproc) ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE KERNELDIR=$KERNELDIR -C $KERNELDIR M=$(pwd) modules'

