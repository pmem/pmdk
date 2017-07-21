/*
 * Pool.cpp
 *
 *  Created on: 14 lip 2017
 *      Author: huber
 */

#include "Pool.h"

Pool *Pool::pongPool = nullptr;

Pool::Pool(const std::string &fileName) {
	if(nvml::obj::pool<GameStruct>::check(fileName, LAYOUT_NAME) == 1){
		pool = nvml::obj::pool<GameStruct>::open(fileName, LAYOUT_NAME);
	}
	else{
		pool = nvml::obj::pool<GameStruct>::create(fileName, LAYOUT_NAME, PMEMOBJ_MIN_POOL * 10);
	}
}

Pool::~Pool() {
	pool.close();
}

nvml::obj::persistent_ptr<Game> Pool::getGame(){
	nvml::obj::persistent_ptr<GameStruct> root = pool.get_root();
	if(root != nullptr){
		if(root->gam == nullptr)
			nvml::obj::transaction::exec_tx(pool, [&]{
				root->gam = nvml::obj::make_persistent<Game>();
			});
	}
	return root->gam;
}

Pool* Pool::getGamePool(){
	if(pongPool == nullptr){
		return getGamePoolFromFile(DEFAULT_POOLFILE_NAME);
	}
	return pongPool;
}

Pool* Pool::getGamePoolFromFile(const std::string &fileName){
	if(pongPool == nullptr)
		pongPool = new Pool(fileName);
	return pongPool;
}

nvml::obj::pool<GameStruct>& Pool::getPoolToTransaction(){
	return pool;
}
