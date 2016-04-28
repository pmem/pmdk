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
 * list.hpp -- Implementation of list
 */

#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/p.hpp>
#include <libpmemobj/make_persistent.hpp>

namespace example_list
{

template<typename T>
class List {
	class List_entry {
		public:
			List_entry() = delete;
			List_entry(nvml::obj::persistent_ptr<List_entry> previous,
					nvml::obj::persistent_ptr<T> value)
			{
				val = value;
				next = nullptr;
				prev = previous;
			}

			nvml::obj::persistent_ptr<List_entry> prev;
			nvml::obj::persistent_ptr<List_entry> next;
			nvml::obj::persistent_ptr<T> val;
	};

	public:
		List()
		{
			head = nullptr;
			tail = head;
			len = 0;
		}

		void push_back(nvml::obj::persistent_ptr<T> val)
		{
			auto tmp = nvml::obj::make_persistent<List_entry>(tail, val);
			if (head == nullptr)
				head = tmp;
			else
				tail->next = tmp;
			tail = tmp;
			len = len + 1;
		}
		nvml::obj::persistent_ptr<T> pop_back()
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
				nvml::obj::delete_persistent<T>(tail->val);
				tail = e->prev;
				remove_elm(e);
			}
		}

		nvml::obj::persistent_ptr<T> get(unsigned id)
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

		nvml::obj::persistent_ptr<List_entry> get_elm(unsigned id)
		{
			if (id >= len)
				return nullptr;
			auto tmp = head;
			for (unsigned i = 0; i < id; i++)
				tmp = tmp->next;
			return tmp;
		}

		int remove_elm(nvml::obj::persistent_ptr<List_entry> elm)
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
			nvml::obj::delete_persistent<List_entry>(elm);
			return 0;
		}

		nvml::obj::p<unsigned> len;
		nvml::obj::persistent_ptr<List_entry> head;
		nvml::obj::persistent_ptr<List_entry> tail;
};
};
