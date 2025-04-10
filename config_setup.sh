#!/bin/bash
##########################################################
# WARNING: This file is not meant to be run directly!    #
# Please follow the Adaptyst installation guide instead. #
##########################################################
if [[ -f $1 ]]; then
    sed -ie "sperf_path=.*perf_path=$2" $1
else
    echo "perf_path=$2" > $1
fi
