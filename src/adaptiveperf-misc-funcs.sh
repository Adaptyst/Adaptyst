#!/bin/bash

# AdaptivePerf: comprehensive profiling tool based on Linux perf
# Copyright (C) 2023 CERN.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; only version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

function convert_from_ns_to_us() {
    while read -ra arr; do
        if [[ ${arr[-1]} == *\# ]]; then
            arr[-1]=${arr[-1]:0:-1}
            overall_offcpu=true
        else
            overall_offcpu=false
        fi

        new_val=$(perl <<< "print ${arr[-1]}/1000")

        if [[ $overall_offcpu == true ]]; then
            new_val+=\#
        fi

        arr[-1]=$new_val

        echo ${arr[@]}
    done
}
