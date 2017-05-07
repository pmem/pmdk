/*
 * Copyright 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pchess.cpp - entry point of persistent chess engine
 */

#include "state.hpp"

#include <cctype>
#include <cstdlib>
#include <unistd.h>

#include <iostream>
#include <string>

#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>

#include <sys/stat.h>

using nvml::obj::pool;
using nvml::obj::transaction;
using nvml::obj::persistent_ptr;
using examples::pchess::state;

typedef pool<persistent_ptr<state>> state_pool;

[[noreturn]] static void usage_exit(const char *progname, int status);
static void print_welcome();

int
main(int argc, char **argv)
{
	using nvml::obj::make_persistent;
	using nvml::obj::delete_persistent;

	static const char *layout_name = "pmem-example-pchess";

	if (argc < 2)
		usage_exit(argv[0], EXIT_FAILURE);

	if (argv[1][0] == '-' and std::tolower(argv[1][1]) == 'h')
		usage_exit(argv[0], EXIT_SUCCESS);

	print_welcome();
	/*
	 * pop - A pmemobj pool
	 * In pchess the global state of the program is stored is
	 * a god object. This is the root object stored in
	 * the pool.
	 */
	state_pool pop;

	/*
	 * Load or create the pool at the given path. There is an
	 * obvious race condition between the access(3) call, and
	 * pool::create/pool::open, but it is ignored for now. It can't
	 * cause data to be corrupted, it might just result in an
	 * error message.
	 */
	if (access(argv[1], F_OK) != 0)
		pop = state_pool::create(argv[1], layout_name, PMEMOBJ_MIN_POOL,
					 S_IRWXU);
	else
		pop = state_pool::open(argv[1], layout_name);

	auto gstate = pop.get_root();

	/*
	 * The gstate object behaves sort of like an FSM.
	 * Currently it does all the work in small increments, each
	 * committed to persistent memory in this loop. This is by far
	 * the most trivial way of handling things, and generally not
	 * what one would do in a database software.
	 * But pchess is mainly for exploring the use the p<>
	 * and the persistent_ptr<> templates.
	 *
	 * Run the loop until either:
	 *  * the operator quits
	 *  * end of input
	 *  * or the process is stopped
	 */
	do {

#if __cpp_lib_uncaught_exceptions || _MSC_VER >= 1900
		/* scoped transaction - the C++17 way */

		transaction::automatic tx(pop);

		/*
		 * Initialize the global state, if this is the first
		 * time pchess runs.
		 */
		if (*gstate == nullptr)
			*gstate = make_persistent<state>();

		(*gstate)->iterate_main_loop(std::cin, std::cout, std::cerr);

#else
		/* transaction in a lambda - the pre C++17 way */

		transaction::exec_tx(pop, [&] {
			/*
			 * Initialize the global state, if this is the first
			 * time pchess runs.
			 */
			if (*gstate == nullptr)
				*gstate = make_persistent<state>();

			(*gstate)->iterate_main_loop(std::cin, std::cout,
						     std::cerr);
		});
#endif
	} while (not gstate[0]->is_session_finished());

	if ((*gstate)->is_finished()) {
		transaction::exec_tx(pop, [&] {
			delete_persistent<state>(*gstate);
			*gstate = nullptr;
		});
	}

	pop.close();

	return EXIT_SUCCESS;
}

static void
print_welcome()
{
	std::cout << "Welcome, this is pchess. To get some help using\n"
		  << " the command line interface, type help<enter>\n";
}

static void
usage_exit(const char *progname, int status)
{
	((status == EXIT_SUCCESS) ? std::cout : std::cerr)
		<< "pchess - A program playing Polish chess variant, called"
		   " przsyczgrzszachy,"
		   " invented by Grzegorz Brzęczyszczykiewicz.\n"
		<< " Just kidding, it is a chess engine"
		   " using persistent memory.\n"
		<< " Usage: " << progname << " path_to_pmem_pool\n";
	std::exit(status);
}
