#!/bin/bash
# script for manual build in case you don't want to use docker-compose
IMAGE_NAME=${IMAGE_NAME:-"er-build"}
EDGEOS_VERSION=${EDGEOS_VERSION:-"2.0.9-2"}

# general build environment image
docker build \
    -t $IMAGE_NAME:latest \
    -t $IMAGE_NAME:$EDGEOS_VERSION \
    --target builder \
    .

# kernel module builder image
MODULE_BUILDER_NAME="${IMAGE_BASENAME}-module"
docker build \
    -t $MODULE_BUILDER_NAME:latest \
    -t $MODULE_BUILDER_NAME:$EDGEOS_VERSION \
    --target module-builder \
    .




