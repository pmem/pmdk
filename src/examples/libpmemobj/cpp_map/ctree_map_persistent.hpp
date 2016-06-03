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

#ifndef EXAMPLES_CTREE_MAP_PERSISTENT_HPP
#define EXAMPLES_CTREE_MAP_PERSISTENT_HPP
#include <cstdint>
#include <functional>
#include <stdlib.h>

#include <libpmemobj/make_persistent.hpp>
#include <libpmemobj/make_persistent_atomic.hpp>
#include <libpmemobj/p.hpp>
#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/transaction.hpp>
#include <libpmemobj/utils.hpp>

#define BIT_IS_SET(n, i) (!!((n) & (1ULL << (i))))

namespace nvobj = nvml::obj;

namespace examples
{

/**
 * C++ implementation of a persistent ctree.
 *
 * Based on the volatile version. This version was implemented to show how much
 * effort is needed to convert a volatile structure into a persistent one using
 * C++ obj bindings. All API functions are atomic in respect to persistency.
 */
template <typename K, typename T>
class ctree_map_p {
public:
	/** Convenience typedef for the key type. */
	typedef K key_type;

	/** Convenience typedef for the value type. */
	typedef nvobj::persistent_ptr<T> value_type;

	/** Convenience typedef for the callback function. */
	typedef std::function<int(key_type, value_type, void *)> callback;

	/**
	 * Default constructor.
	 */
	ctree_map_p()
	{
		auto pop = nvobj::pool_by_vptr(this);

		nvobj::transaction::exec_tx(
			pop, [&] { root = nvobj::make_persistent<entry>(); });
	}

	/**
	 * Insert or update the given value under the given key.
	 *
	 * The map takes ownership of the value.
	 *
	 * @param key The key to insert under.
	 * @param value The value to be inserted.
	 *
	 * @return 0 on success, negative values on error.
	 */
	int
	insert(uint64_t key, value_type value)
	{
		auto dest_entry = root;
		while (dest_entry->inode != nullptr) {
			auto n = dest_entry->inode;
			dest_entry = n->entries[BIT_IS_SET(key, n->diff)];
		}

		entry e(key, value);
		auto pop = nvobj::pool_by_vptr(this);
		nvobj::transaction::exec_tx(pop, [&] {
			if (dest_entry->key == 0 || dest_entry->key == key) {
				nvobj::delete_persistent<T>(dest_entry->value);
				*dest_entry = e;
			} else {
				insert_leaf(&e, find_crit_bit(dest_entry->key,
							      key));
			}
		});

		return 0;
	}

	/**
	 * Allocating insert.
	 *
	 * Creates a new value_type instance and inserts it into the tree.
	 *
	 * @param key The key to insert under.
	 * @param args variadic template parameter for object construction
	 *	arguments.
	 *
	 * @return 0 on success, negative values on error.
	 */
	template <typename... Args>
	int
	insert_new(key_type key, const Args &... args)
	{
		auto pop = nvobj::pool_by_vptr(this);
		nvobj::transaction::exec_tx(pop, [&] {
			return insert(key, nvobj::make_persistent<T>(args...));
		});

		return -1;
	}

	/**
	 * Remove a value from the tree.
	 *
	 * The tree no longer owns the value.
	 *
	 * @param key The key for which the value will be removed.
	 *
	 * @return The value if it is in the tree, nullptr otherwise.
	 */
	value_type
	remove(key_type key)
	{
		nvobj::persistent_ptr<entry> parent = nullptr;
		auto leaf = get_leaf(key, &parent);

		if (leaf == nullptr)
			return nullptr;

		auto ret = leaf->value;

		auto pop = nvobj::pool_by_vptr(this);
		nvobj::transaction::exec_tx(pop, [&] {
			if (parent == nullptr) {
				leaf->key = 0;
				leaf->value = nullptr;
			} else {
				auto n = parent->inode;
				*parent = *(
					n->entries[parent->inode->entries[0]
							   ->key == leaf->key]);

				/* cleanup entries and the unnecessary node */
				nvobj::delete_persistent<entry>(n->entries[0]);
				nvobj::delete_persistent<entry>(n->entries[1]);
				nvobj::delete_persistent<node>(n);
			}
		});

		return ret;
	}

	/**
	 * Remove entry from tree and deallocate it.
	 *
	 * @param key The key denoting the entry to be removed.
	 *
	 * @return 0 on success, negative values on error.
	 */
	int
	remove_free(key_type key)
	{
		auto pop = nvobj::pool_by_vptr(this);
		nvobj::transaction::exec_tx(
			pop, [&] { nvobj::delete_persistent<T>(remove(key)); });
		return 0;
	}

	/**
	 * Clear the tree and deallocate all entries.
	 */
	int
	clear()
	{
		auto pop = nvobj::pool_by_vptr(this);
		nvobj::transaction::exec_tx(pop, [&] {
			if (root->inode) {
				root->inode->clear();
				nvobj::delete_persistent<node>(root->inode);
				root->inode = nullptr;
			}

			nvobj::delete_persistent<T>(root->value);
			root->value = nullptr;
			root->key = 0;
		});
		return 0;
	}

	/**
	 * Return the value from the tree for the given key.
	 *
	 * @param key The key for which the value will be returned.
	 *
	 * @return The value if it is in the tree, nullptr otherwise.
	 */
	value_type
	get(key_type key)
	{
		auto ret = get_leaf(key, nullptr);

		return ret ? ret->value : nullptr;
	}

	/**
	 * Check if an entry for the given key is in the tree.
	 *
	 * @param key The key to check.
	 *
	 * @return 0 on
	 */
	int
	lookup(key_type key)
	{
		return get(key) != nullptr;
	}

	/**
	 * Call clb for each element in the tree.
	 *
	 * @param clb The callback to be called.
	 * @param args The arguments forwarded to the callback.
	 *
	 * @return 0 if tree empty, clb return value otherwise.
	 */
	int foreach (callback clb, void *args)
	{
		if (is_empty())
			return 0;

		return foreach_node(root, clb, args);
	}

	/**
	 * Check if tree is empty.
	 *
	 * @return 1 if empty, 0 otherwise.
	 */
	int
	is_empty()
	{
		return root->value == nullptr && root->inode == nullptr;
	}

	/**
	 * Check tree consistency.
	 *
	 * @return 0 on success, negative values on error.
	 */
	int
	check()
	{
		return 0;
	}

	/**
	 * Destructor.
	 */
	~ctree_map_p()
	{
		clear();
	}

private:
	struct node;

	/*
	 * Entry holding the value.
	 */
	struct entry {
		entry() : key(0), inode(nullptr), value(nullptr)
		{
		}

		entry(key_type _key, value_type _value)
		    : key(_key), inode(nullptr), value(_value)
		{
		}

		nvobj::p<key_type> key;
		nvobj::persistent_ptr<node> inode;
		value_type value;

		void
		clear()
		{
			if (inode) {
				inode->clear();
				nvobj::delete_persistent<node>(inode);
				inode = nullptr;
			}
			nvobj::delete_persistent<T>(value);
			value = nullptr;
		}
	};

	/*
	 * Internal node pointing to two entries.
	 */
	struct node {
		node() : diff(0)
		{
			entries[0] = nullptr;
			entries[1] = nullptr;
		}

		nvobj::p<int> diff; /* most significant differing bit */
		nvobj::persistent_ptr<entry> entries[2];

		void
		clear()
		{
			if (entries[0]) {
				entries[0]->clear();
				nvobj::delete_persistent<entry>(entries[0]);
				entries[0] = nullptr;
			}
			if (entries[1]) {
				entries[1]->clear();
				nvobj::delete_persistent<entry>(entries[1]);
				entries[1] = nullptr;
			}
		}
	};

	/*
	 * Find critical bit.
	 */
	static int
	find_crit_bit(key_type lhs, key_type rhs)
	{
		return 64 - __builtin_clzll(lhs ^ rhs) - 1;
	}

	/*
	 * Insert leaf into the tree.
	 */
	void
	insert_leaf(const entry *e, int diff)
	{
		auto new_node = nvobj::make_persistent<node>();
		new_node->diff = diff;

		int d = BIT_IS_SET(e->key, new_node->diff);
		new_node->entries[d] = nvobj::make_persistent<entry>(*e);

		auto dest_entry = root;
		while (dest_entry->inode != nullptr) {
			auto n = dest_entry->inode;
			if (n->diff < new_node->diff)
				break;

			dest_entry = n->entries[BIT_IS_SET(e->key, n->diff)];
		}

		new_node->entries[!d] =
			nvobj::make_persistent<entry>(*dest_entry);
		dest_entry->key = 0;
		dest_entry->inode = new_node;
		dest_entry->value = nullptr;
	}

	/*
	 * Fetch leaf from the tree.
	 */
	nvobj::persistent_ptr<entry>
	get_leaf(uint64_t key, nvobj::persistent_ptr<entry> *parent)
	{
		auto n = root;
		nvobj::persistent_ptr<entry> p = nullptr;

		while (n->inode != nullptr) {
			p = n;
			n = n->inode->entries[BIT_IS_SET(key, n->inode->diff)];
		}

		if (n->key == key) {
			if (parent)
				*parent = p;

			return n;
		}

		return nullptr;
	}

	/*
	 * Recursive foreach on nodes.
	 */
	int
	foreach_node(const nvobj::persistent_ptr<entry> e, callback clb,
		     void *arg)
	{
		int ret = 0;

		if (e->inode != nullptr) {
			auto n = e->inode;
			if (foreach_node(n->entries[0], clb, arg) == 0)
				foreach_node(n->entries[1], clb, arg);
		} else {
			ret = clb(e->key, e->value, arg);
		}

		return ret;
	}

	/* Tree root */
	nvobj::persistent_ptr<entry> root;
};

} /* namespace examples */

#endif /* EXAMPLES_CTREE_MAP_PERSISTENT_HPP */
