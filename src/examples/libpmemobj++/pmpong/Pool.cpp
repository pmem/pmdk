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

Pool *Pool::pongPool = nullptr;

Pool::Pool(const std::string &fileName)
{
	if (pmem::obj::pool<GameStruct>::check(fileName, LAYOUT_NAME) == 1) {
		pool = pmem::obj::pool<GameStruct>::open(fileName, LAYOUT_NAME);
	} else {
		pool = pmem::obj::pool<GameStruct>::create(
			fileName, LAYOUT_NAME, PMEMOBJ_MIN_POOL * 6);
	}
}

Pool::~Pool()
{
	pool.close();
}

Pool *
Pool::getGamePoolFromFile(const std::string &fileName)
{
	if (pongPool == nullptr)
		pongPool = new Pool(fileName);
	return pongPool;
}

Pool *
Pool::getGamePool()
{
	if (pongPool == nullptr) {
		return getGamePoolFromFile(DEFAULT_POOLFILE_NAME);
	}
	return pongPool;
}

pmem::obj::persistent_ptr<GameController>
Pool::getGameController()
{
	pmem::obj::persistent_ptr<GameStruct> root = pool.get_root();
	if (root != nullptr) {
		if (root->gam == nullptr)
			pmem::obj::transaction::exec_tx(pool, [&] {
				root->gam = pmem::obj::make_persistent<
					GameController>();
			});
	}
	return root->gam;
}

pmem::obj::pool<GameStruct> &
Pool::getPoolToTransaction()
{
	return pool;
}
