/*
 * Copyright 2017, Intel Corporation
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

#include "Pool.hpp"
#include <SFML/Graphics.hpp>
#include <cstdio>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	int exitCode = EXIT_FAILURE;
	std::string mode = "";
	if (argc == 3) {
		mode = argv[2];
	}
	if (argc < 2 || argc > 3 || (argc == 3 && mode != "-s")) {
		std::cout << "Usage: ./pmpong <game_session_file> "
			     "[options]"
			  << std::endl
			  << "Options: " << std::endl
			  << "-s, simulates game between 2 AI players"
			  << std::endl;
		return exitCode;
	}
	std::string fileName = argv[1];
	try {
		Pool *pool = Pool::getGamePoolFromFile(fileName);
		pmem::obj::persistent_ptr<GameController> gameController =
			pool->getGameController();
		if (mode == "-s")
			gameController->gameLoop(true);
		else
			gameController->gameLoop();
		delete pool;
		exitCode = EXIT_SUCCESS;
	} catch (pmem::transaction_error &err) {
		std::cerr << err.what() << std::endl;
	} catch (pmem::transaction_scope_error &tse) {
		std::cerr << tse.what() << std::endl;
	} catch (pmem::pool_error &pe) {
		std::cerr << pe.what() << std::endl;
	} catch (std::logic_error &le) {
		std::cerr << le.what() << std::endl;
	} catch (std::exception &exc) {
		std::cerr << exc.what() << std::endl;
	}

	return exitCode;
}
