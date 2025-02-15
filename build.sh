#!/bin/bash

# exit on error
set -e 

# create build directory
BUILD_DIR="build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# run cmake to generate makefiles
cmake  ..

# compile the project
make -j$(nproc)

# return to the root directory
cd ..
echo "Build complete. Binaries installed to $BUILD_DIR/ directory"