#!/bin/bash

# Adaptyst: a performance analysis tool
# Copyright (C) CERN. See LICENSE for details.

set -e

function echo_main() {
    if [[ $2 -eq 1 ]]; then
        echo -e "\033[1;31m==> $1\033[0m"
    else
        echo -e "\033[1;32m==> $1\033[0m"
    fi
}

function echo_sub() {
    if [[ $2 -eq 1 ]]; then
        echo -e "\033[0;31m-> $1\033[0m"
    else
        echo -e "\033[0;34m-> $1\033[0m"
    fi
}

function error() {
    echo_main "An error has occurred!" 1
    exit 2
}

trap "error" ERR

if [[ $1 == "-h" || $1 == "--help" ]]; then
    echo "Script for building Adaptyst."
    echo "Usage: ./build.sh [optional CMake options]"
    exit 0
fi

echo_main "Building Adaptyst and adaptyst-server..."

if [[ -d build ]]; then
    echo_sub "Non-empty build dir detected! Recompiling."
    echo_sub "Optional CMake flags will be ignored. If it's not desired, please run clean.sh first."

    cd build
    ./make.sh
else
    mkdir build
    echo "#!/bin/bash" > build/make.sh
    echo "cmake --build . && mv adaptyst libadaptystserv.so adaptyst-server ../" >> build/make.sh
    chmod +x build/make.sh

    cd build
    cmake .. $@
    cmake --build .
    mv adaptyst ../
    mv libadaptystserv.so adaptyst-server ../
fi

echo_main "Done! You can run install.sh now."
