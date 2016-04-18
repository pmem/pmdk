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

#include <libpmemobj/make_persistent.hpp>
#include <libpmemobj/p.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pext.hpp>

namespace examples
{

template <typename T>
class list {
	class list_entry {
	public:
		list_entry() = delete;
		list_entry(nvml::obj::persistent_ptr<list_entry> previous,
			   nvml::obj::persistent_ptr<T> value)
		{
			val = value;
			next = nullptr;
			prev = previous;
		}

		nvml::obj::persistent_ptr<list_entry> prev;
		nvml::obj::persistent_ptr<list_entry> next;
		nvml::obj::persistent_ptr<T> val;
	};

public:
	list()
	{
		head = nullptr;
		tail = head;
		len = 0;
	}

	/**
	 * Push back the new element.
	 */
	void
	push_back(nvml::obj::persistent_ptr<T> val)
	{
		auto tmp = nvml::obj::make_persistent<list_entry>(tail, val);
		if (head == nullptr)
			head = tmp;
		else
			tail->next = tmp;
		tail = tmp;
		++len;
	}

	/**
	 * Pop the last element out from the list and return
	 * the pointer to it
	 */
	nvml::obj::persistent_ptr<T>
	pop_back()
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

	/**
	 * Return the pointer to the next element
	 */
	nvml::obj::persistent_ptr<list_entry>
	erase(unsigned id)
	{
		return remove_elm(get_elm(id));
	}

	/* clear - clear the whole list */
	void
	clear()
	{
		while (head != nullptr) {
			auto e = head;
			head = remove_elm(e);
		}
	}

	/**
	 * Get element with given id in list
	 */
	nvml::obj::persistent_ptr<T>
	get(unsigned id)
	{
		auto elm = get_elm(id);
		if (elm == nullptr)
			return nullptr;
		return elm->val;
	}

	/**
	 * Return number of elements in list
	 */
	unsigned
	size()
	{
		return len;
	}

private:
	nvml::obj::persistent_ptr<list_entry>
	get_elm(unsigned id)
	{
		if (id >= len)
			return nullptr;
		auto tmp = head;
		for (unsigned i = 0; i < id; i++)
			tmp = tmp->next;
		return tmp;
	}

	nvml::obj::persistent_ptr<list_entry>
	remove_elm(nvml::obj::persistent_ptr<list_entry> elm)
	{
		assert(elm != nullptr);
		auto tmp = elm->next;
		nvml::obj::delete_persistent<T>(elm->val);

		/* removing item is head */
		if (elm == head)
			head = elm->next;
		else
			elm->prev->next = elm->next;

		/* removing item is tail */
		if (elm == tail)
			tail = elm->prev;
		else
			elm->next->prev = elm->prev;

		--len;
		nvml::obj::delete_persistent<list_entry>(elm);
		return tmp;
	}

	nvml::obj::p<unsigned> len;
	nvml::obj::persistent_ptr<list_entry> head;
	nvml::obj::persistent_ptr<list_entry> tail;
};
};
