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

#ifndef EXAMPLES_CTREE_MAP_VOLATILE_HPP
#define EXAMPLES_CTREE_MAP_VOLATILE_HPP
#include <cstdint>
#include <functional>
#include <stdlib.h>

#define BIT_IS_SET(n, i) (!!((n) & (1ULL << (i))))

namespace examples
{

/**
 * C++ implementation of a volatile ctree.
 *
 * Based on the C implementation.
 */
template <typename K, typename T>
class ctree_map_transient {
public:
	/** Convenience typedef for the key type. */
	typedef K key_type;

	/** Convenience typedef for the value type. */
	typedef T *value_type;

	/** Convenience typedef for the callback function. */
	typedef std::function<int(key_type, value_type, void *)> callback;

	/**
	 * Default constructor.
	 */
	ctree_map_transient() : root(new entry())
	{
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
		if (dest_entry->key == 0 || dest_entry->key == key) {
			delete dest_entry->value;
			*dest_entry = e;
		} else {
			insert_leaf(&e, ctree_map_transient::find_crit_bit(
						dest_entry->key, key));
		}

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
		return insert(key, new T(args...));
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
		entry *parent = nullptr;
		auto leaf = get_leaf(key, &parent);

		if (leaf == nullptr)
			return nullptr;

		auto ret = leaf->value;

		if (parent == nullptr) {
			leaf->key = 0;
			leaf->value = nullptr;
		} else {
			auto n = parent->inode;
			*parent = *(n->entries[parent->inode->entries[0]->key ==
					       leaf->key]);

			/* cleanup both entries and the unnecessary node */
			delete n->entries[0];
			delete n->entries[1];
			delete n;
		}

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
		delete remove(key);
		return 0;
	}

	/**
	 * Clear the tree and deallocate all entries.
	 */
	int
	clear()
	{
		if (root->inode) {
			root->inode->clear();
			delete root->inode;
			root->inode = nullptr;
		}

		delete root->value;
		root->value = nullptr;
		root->key = 0;
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
	~ctree_map_transient()
	{
		clear();
		delete root;
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

		key_type key;
		node *inode;
		value_type value;

		/*
		 * Clear the entry.
		 */
		void
		clear()
		{
			if (inode) {
				inode->clear();
				delete inode;
			}
			delete value;
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

		int diff; /* most significant differing bit */
		entry *entries[2];

		/*
		 * Clear the node.
		 */
		void
		clear()
		{
			if (entries[0]) {
				entries[0]->clear();
				delete entries[0];
			}
			if (entries[1]) {
				entries[1]->clear();
				delete entries[1];
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
		auto new_node = new node();
		new_node->diff = diff;

		int d = BIT_IS_SET(e->key, new_node->diff);
		new_node->entries[d] = new entry(*e);

		auto dest_entry = root;
		while (dest_entry->inode != nullptr) {
			auto n = dest_entry->inode;
			if (n->diff < new_node->diff)
				break;

			dest_entry = n->entries[BIT_IS_SET(e->key, n->diff)];
		}

		new_node->entries[!d] = new entry(*dest_entry);
		dest_entry->key = 0;
		dest_entry->inode = new_node;
		dest_entry->value = nullptr;
	}

	/*
	 * Fetch leaf from the tree.
	 */
	entry *
	get_leaf(uint64_t key, entry **parent)
	{
		auto n = root;
		entry *p = nullptr;

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
	foreach_node(const entry *e, callback clb, void *arg)
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
	entry *root;
};

} /* namespace examples */

#endif /* EXAMPLES_CTREE_MAP_VOLATILE_HPP */
