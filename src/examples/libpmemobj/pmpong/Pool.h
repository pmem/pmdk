/*
 * Pool.h
 *
 *  Created on: 14 lip 2017
 *      Author: huber
 */

#ifndef POOL_H_
#define POOL_H_
#include <SFML/Graphics.hpp>
#include <string>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include "GameController.h"


struct GameStruct {
public:
	nvml::obj::persistent_ptr<GameController> gam;
};

class Pool {

public:
	~Pool();
	static Pool *getGamePoolFromFile(const std::string &fileName);
	static Pool *getGamePool();
	nvml::obj::persistent_ptr<GameController> getGameController();
	nvml::obj::pool<GameStruct> &getPoolToTransaction();

private:
	Pool(const std::string &name);
	static Pool *pongPool;

	nvml::obj::pool<GameStruct> pool;

	Pool(const Pool&);
	Pool& operator=(const Pool&);

};

#endif /* POOL_H_ */
