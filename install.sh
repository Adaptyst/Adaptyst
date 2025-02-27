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
    echo "Script for installing Adaptyst tools after building them."
    echo "Usage: ./install.sh [optional installation prefix]"
    echo "Default prefix is /usr/local."
    exit 0
fi

if [[ ! -f adaptyst-server || ! -f libadaptystserv.so ]]; then
    echo_main "No adaptyst-server and/or libadaptystserv.so detected!" 1
    echo_main "Please put them inside this directory or run build.sh." 1
    exit 1
fi

if [[ $1 == "" ]]; then
    prefix=/usr/local
else
    prefix=$1
fi

mkdir -p $prefix/bin $prefix/lib

if [[ -f adaptyst ]]; then
    if [[ -f build/CMakeCache.txt ]]; then
        echo_main "Compiling and installing Adaptyst-patched \"perf\"..."
        aperf_prefix=$(sed -rn 's/APERF_SCRIPT_PATH.*=(.*)/\1/p' build/CMakeCache.txt)
        aperf_config=$(sed -rn 's/APERF_CONFIG_PATH.*=(.*)/\1/p' build/CMakeCache.txt)

        if [[ $aperf_prefix == "" ]]; then
            echo_sub "Could not find APERF_SCRIPT_PATH in build/CMakeCache.txt!" 1
            exit 2
        fi

        if [[ $aperf_config == "" ]]; then
            echo_sub "Could not find APERF_CONFIG_PATH in build/CMakeCache.txt!" 1
            exit 2
        fi

        if [[ $aperf_prefix == */ ]]; then
            aperf_perf_prefix="${aperf_prefix}perf"
        else
            aperf_perf_prefix="$aperf_prefix/perf"
        fi

        echo_sub "Adaptyst-patched \"perf\" will be installed in $aperf_perf_prefix."
        echo_sub "Press any key to continue or Ctrl-C to cancel."

        read -srn 1

        mkdir -p "$aperf_perf_prefix"

        if [[ ! -d linux/tools/perf ]]; then
            echo_sub "linux submodule seems to be missing, pulling it..."
            git submodule update --init --force --depth 1
        fi

        old_dir=$(pwd)
        cd linux/tools/perf
        make install BUILD_BPF_SKEL=1 prefix="$aperf_perf_prefix"
        cd $old_dir

        echo_main "Installing adaptyst..."
        cp adaptyst $prefix/bin
        echo "perf_path=$aperf_perf_prefix" > "$aperf_config"

        echo_main "Installing Adaptyst \"perf\" scripts..."
        cd src/scripts
        make install prefix="$aperf_prefix"
        cd $old_dir
    else
        echo_main "Adaptyst cannot be installed because there's no CMakeCache.txt in build dir!" 1
    fi
fi

echo_main "Installing adaptyst-server..."
cp adaptyst-server $prefix/bin
cp libadaptystserv.so $prefix/lib

if ! ldconfig; then
    if [[ "$EUID" == "0" ]]; then
        echo_sub "ldconfig has failed! You may get \"libadaptystserv.so not found\" errors when you run Adaptyst or adaptyst-server." 1
        exit 3
    else
        echo_sub "ldconfig has failed as you're not running the script as root."
        echo_sub "If you use a non-system prefix, you can ignore this (don't forget to set LD_LIBRARY_PATH)."
        echo_sub "Otherwise, run ldconfig as root."
    fi
fi

echo_main "Installing Adaptyst utilities..."
old_dir=$(pwd)
cd src/utils
make install prefix="$prefix"
cd $old_dir

if [[ -f adaptyst ]]; then
    echo_main "Done! You can use Adaptyst and adaptyst-server now + the utilities with the \"adaptyst-\" prefix."
    echo_main "For example, run \"adaptyst --help\", \"adaptyst-server --help\", or \"adaptyst-code --help\"."
else
    echo_main "Done! You can use adaptyst-server now + the utilities with the \"adaptyst-\" prefix."
    echo_main "For example, run \"adaptyst-server --help\" or \"adaptyst-code --help\"."
fi
