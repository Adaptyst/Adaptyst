#!/bin/bash
set -e

# This script is not meant to be run directly!
# Please use "make install prefix=<install prefix>" or "make uninstall".

if [[ "$1" == "uninstall" ]]; then
    if [[ -f prefix.txt ]]; then
        prefix=$(cat prefix.txt)
        rm "$prefix"/adaptyst-*.py
        rm prefix.txt
    else
        echo "No prefix.txt found! Have you installed the scripts before?"
        exit 1
    fi
else
    cp adaptyst-syscall-process.py adaptyst-process.py "$1"
    echo "$1" > prefix.txt
fi
