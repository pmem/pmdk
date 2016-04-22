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
 * pman_list.hpp -- list used in pman.cpp
 */

#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/p.hpp>
#include <libpmemobj/make_persistent.hpp>

using namespace nvml::obj;

template<typename T>
class pman_list {
	class list_entry {
		public:
			list_entry();
			list_entry(persistent_ptr<list_entry> previous,
							persistent_ptr<T> value)
			{
				val = value;
				next = nullptr;
				prev = previous;
			}

			persistent_ptr<list_entry> prev;
			persistent_ptr<list_entry> next;
			persistent_ptr<T> val;
	};

	public:
		pman_list()
		{
			head = nullptr;
			tail = head;
		}

		void push_back(persistent_ptr<T> val)
		{
			auto tmp = make_persistent<list_entry>(tail, val);
			if (head == nullptr)
				head = tmp;
			else
				tail->next = tmp;
			tail = tmp;
			len = len + 1;
		}
		persistent_ptr<T> pop_back()
		{
			assert(head != nullptr);
			auto tmp = tail;
			tail = tmp->prev;
			if (tail == nullptr)
				head = tail;
			else
				tail->next = nullptr;
			return tmp->val;
		}

		void erase(unsigned id)
		{
			remove_elm(get_elm(id));

		}

		void clear()
		{
			while (tail != nullptr) {
				auto e = tail;
				delete_persistent<T>(tail->val);
				tail = e->prev;
				remove_elm(e);
			}
		}

		persistent_ptr<T> get(unsigned id)
		{
			auto elm = get_elm(id);
			if (elm == nullptr)
				return nullptr;
			return elm->val;
		}

		unsigned size()
		{
			return len;
		}

	private:

		persistent_ptr<list_entry> get_elm(unsigned id)
		{
			if (id >= len)
				return nullptr;
			auto tmp = head;
			for (unsigned i = 0; i < id; i++)
				tmp = tmp->next;
			return tmp;
		}

		int remove_elm(persistent_ptr<list_entry> elm)
		{
			assert(elm != nullptr);

			/* removing item is head */
			if (elm->prev != nullptr)
				elm->prev->next = elm->next;
			else
				head = elm->next;

			/* removing item is tail */
			if (elm->next != nullptr)
				elm->next->prev = elm->prev;
			else
				tail = elm->prev;

			len = len - 1;
			delete_persistent<list_entry>(elm);
			return 0;
		}

		p<unsigned> len;
		persistent_ptr<list_entry> head;
		persistent_ptr<list_entry> tail;
};
