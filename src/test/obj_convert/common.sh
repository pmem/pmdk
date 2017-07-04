#!/bin/bash -e
#
# Copyright 2015-2017, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# src/test/obj_convert/common.sh -- common part of conversion tool tests
#

# exits in the middle of transaction, so pool cannot be closed
export MEMCHECK_DONT_CHECK_LEAKS=1

verify_scenario_old() {
	# convert tool always ask for confirmation, so say yes ;)
	yes | expect_normal_exit\
		$PMEMPOOL$EXESUFFIX convert $DIR/scenario$1a &> /dev/null
	expect_normal_exit ./obj_convert$EXESUFFIX $DIR/scenario$1a va $1

	yes | expect_normal_exit\
		$PMEMPOOL$EXESUFFIX convert $DIR/scenario$1c &> /dev/null
	expect_normal_exit ./obj_convert$EXESUFFIX $DIR/scenario$1c vc $1
}

verify_scenario_head() {
	expect_normal_exit ./obj_convert$EXESUFFIX $DIR/scenario$1a va $1
	expect_normal_exit ./obj_convert$EXESUFFIX $DIR/scenario$1c vc $1
}

create_scenario() {
	LD_LIBRARY_PATH=$1 gdb --batch\
		--command=trip_on_pre_commit.gdb --args\
		./obj_convert$EXESUFFIX $DIR/scenario$2a c $2 &> /dev/null

	LD_LIBRARY_PATH=$1 gdb --batch\
		--command=trip_on_post_commit.gdb --args\
		./obj_convert$EXESUFFIX $DIR/scenario$2c c $2 &> /dev/null
}

#PATH_TO_1_0_DBG=/../nvml/src/debug
create_scenario_1_0() {
	create_scenario $PATH_TO_1_0_DBG $1
}

#PATH_TO_1_2_DBG=/../nvml2/src/debug
create_scenario_1_2() {
	create_scenario $PATH_TO_1_2_DBG $1
}

create_scenario_head() {
	create_scenario $TEST_LD_LIBRARY_PATH $1
}

clear_scenarios() {
	sc=("$@")

	for i in "${sc[@]}"
	do
		rm -rf $DIR/scenario$1a
		rm -rf $DIR/scenario$1c
	done
}

run_scenarios_head() {
	sc=("$@")

	for i in "${sc[@]}"
	do
		create_scenario_head $i
	done

	for i in "${sc[@]}"
	do
		verify_scenario_head $i
	done
}

run_scenarios_1_0() {
	sc=("$@")

	if [ -z ${PATH_TO_1_0_DBG+x} ];
	then
		tar -xzf pools.tar.gz -C $DIR
	else
		for i in "${sc[@]}"
		do
			create_scenario_1_0 $i
		done
	fi

	for i in "${sc[@]}"
	do
		verify_scenario_old $i
	done
}

run_scenarios_1_2() {
	sc=("$@")

	if [ -z ${PATH_TO_1_2_DBG+x} ];
	then
		tar -xzf pools_1_2.tar.gz -C $DIR
	else
		for i in "${sc[@]}"
		do
			create_scenario_1_2 $i
		done
	fi

	for i in "${sc[@]}"
	do
		verify_scenario_old $i
	done
}

run_scenarios() {
	run_scenarios_head $@
	clear_scenarios $@

	run_scenarios_1_0 $@
	clear_scenarios $@

	run_scenarios_1_2 $@
	clear_scenarios $@
}
