//============================================================================
// Name        : PmemONG.cpp
// Author      : H.Lopusinski
// Version     :
// Copyright   : IC
// Description : nvml pmemobj example
//============================================================================

#include <SFML/Graphics.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <cstdio>
#include "Pool.h"

int main(int argc, char *argv[]) {
	int exitCode = EXIT_FAILURE;
	std::string mode = "";
	if(argc < 2 || argc > 3){
		std::cout << "CORRECT FORMAT IS: ./PmemONG <game_session_file> [mode]" << std::endl;
		return exitCode;
	}
	if(argc == 3){
		mode = argv[2];
		if(mode != "-s"){
			std::cout << "MODE MIGHT BE ONLY -s" << std::endl;
			return exitCode;
		}
	}
	std::string fileName = argv[1];
	try{
		Pool *pool;
		pool = pool->getGamePoolFromFile(fileName);
		nvml::obj::persistent_ptr<GameController> gameController = pool->getGameController();
		if(mode == "-s")
			gameController->gameLooping(true);
		else
			gameController->gameLooping();
		delete pool;
		exitCode = EXIT_SUCCESS;
	}
	catch (nvml::transaction_error &err){
		std::cerr << err.what() << std::endl;
	}
	catch (nvml::transaction_scope_error &tse){
		std::cerr << tse.what() << std::endl;
	}
	catch (nvml::pool_error &pe) {
		std::cerr << pe.what() << std::endl;
	}
	catch (std::logic_error &le) {
		std::cerr << le.what() << std::endl;
	}
	catch (std::exception &exc){
		std::cerr << exc.what() << std::endl;
	}
	return exitCode;
}




