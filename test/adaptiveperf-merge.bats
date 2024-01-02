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

setup() {
    bats_require_minimum_version 1.5.0
    load test_helper/bats-support/load.bash
    load test_helper/bats-assert/load.bash
    load assert_funcs.bash
}

teardown() {
    rm -f overall_offcpu_collapsed.data
}

@test "[Test 1] 2 parts, no overall off-CPU data" {
    run -0 ./adaptiveperf-merge test/adaptiveperf-merge/no_overall_offcpu1.data test/adaptiveperf-merge/no_overall_offcpu2.data
    
    assert_file_exists overall_offcpu_collapsed.data
    assert_equal "$output" "$(cat test/adaptiveperf-merge/test1_result_expected.data)"
    assert_equal "$(cat overall_offcpu_collapsed.data)" ""
}

@test "[Test 2] 3 parts, with overall off-CPU data" {
    run -0 ./adaptiveperf-merge test/adaptiveperf-merge/with_overall_offcpu1.data test/adaptiveperf-merge/with_overall_offcpu2.data test/adaptiveperf-merge/with_overall_offcpu3.data

    assert_file_exists overall_offcpu_collapsed.data
    assert_equal "$output" "$(cat test/adaptiveperf-merge/test2_result_expected.data)"
    assert_equal "$(cat overall_offcpu_collapsed.data)" "$(cat test/adaptiveperf-merge/test2_overall_offcpu_collapsed_expected.data)"
}

@test "[Test 3] 1 part, no overall off-CPU data" {
    run -0 ./adaptiveperf-merge test/adaptiveperf-merge/no_overall_offcpu1.data

    assert_file_exists overall_offcpu_collapsed.data
    assert_equal "$output" "$(cat test/adaptiveperf-merge/test3_result_expected.data)"
    assert_equal "$(cat overall_offcpu_collapsed.data)" ""
}

@test "[Test 4] 1 part, with overall off-CPU data" {
    run -0 ./adaptiveperf-merge test/adaptiveperf-merge/with_overall_offcpu1.data

    assert_file_exists overall_offcpu_collapsed.data
    assert_equal "$output" "$(cat test/adaptiveperf-merge/test4_result_expected.data)"
    assert_equal "$(cat overall_offcpu_collapsed.data)" "$(cat test/adaptiveperf-merge/test4_overall_offcpu_collapsed_expected.data)"
}

@test "[Test 5] 1 empty part" {
    run -0 ./adaptiveperf-merge test/adaptiveperf-merge/empty.data

    assert_file_exists overall_offcpu_collapsed.data
    assert_equal "$output" ""
    assert_equal "$(cat overall_offcpu_collapsed.data)" ""
}

@test "[Test 6 using test 1 files] 2 parts and 1 empty part, no overall off-CPU data" {
    run -0 ./adaptiveperf-merge test/adaptiveperf-merge/no_overall_offcpu1.data test/adaptiveperf-merge/no_overall_offcpu2.data test/adaptiveperf-merge/empty.data
    
    assert_file_exists overall_offcpu_collapsed.data
    assert_equal "$output" "$(cat test/adaptiveperf-merge/test1_result_expected.data)"
    assert_equal "$(cat overall_offcpu_collapsed.data)" ""
}

@test "[Test 7 using test 2 files] 3 parts and 1 empty part, with overall off-CPU data" {
    run -0 ./adaptiveperf-merge test/adaptiveperf-merge/with_overall_offcpu1.data test/adaptiveperf-merge/with_overall_offcpu2.data test/adaptiveperf-merge/with_overall_offcpu3.data test/adaptiveperf-merge/empty.data

    assert_file_exists overall_offcpu_collapsed.data
    assert_equal "$output" "$(cat test/adaptiveperf-merge/test2_result_expected.data)"
    assert_equal "$(cat overall_offcpu_collapsed.data)" "$(cat test/adaptiveperf-merge/test2_overall_offcpu_collapsed_expected.data)"
}
