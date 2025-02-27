#!/bin/bash

# Adaptyst: a performance analysis tool
# Copyright (C) CERN. See LICENSE for details.

if [[ $1 == "-h" || $1 == "--help" ]]; then
    echo "Script for cleaning all Adaptyst build files."
    echo "Usage: ./clean.sh"
    exit 0
fi

set -v
rm -f adaptyst
rm -f libadaptystserv.so adaptyst-server
rm -rf build
