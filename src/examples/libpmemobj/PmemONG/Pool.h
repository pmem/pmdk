/*
 * Pool.h
 *
 *  Created on: 14 lip 2017
 *      Author: huber
 */

#ifndef POOL_H_
#define POOL_H_
#include <string>
#include "Game.h"
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/persistent_ptr.hpp>


struct GameStruct{
public:
	nvml::obj::persistent_ptr<Game> gam;
};

class Pool {

public:
	~Pool();
	static Pool *getGamePoolFromFile(const std::string &fileName);
	static Pool *getGamePool();
	nvml::obj::persistent_ptr<Game> getGame();
	nvml::obj::pool<GameStruct> &getPoolToTransaction();

private:
	Pool(const std::string &name);
	static Pool *pongPool;

	nvml::obj::pool<GameStruct> pool;

	Pool(const Pool&);
	Pool& operator=(const Pool&);

};

#endif /* POOL_H_ */
