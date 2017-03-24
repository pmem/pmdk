/*
 * Copyright 2017, FUJITSU TECHNOLOGY SOLUTIONS GMBH
 * Copyright 2012, Armon Dadgar. All rights reserved.
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
 * ===========================================================================
 *
 *       Filename:  art.h
 *
 *    Description:  header file for ART tree using libpmemobj based on libart
 *
 *       Author:  Andreas Bluemle, Dieter Kasper
 *                andreas.bluemle@itxperts.de
 *                dieter.kasper@ts.fujitsu.com
 *
 * Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
 *
 * ===========================================================================
 */
/*
 * based on https://github.com/armon/libart/src/art.h
 */
#include <stdint.h>
#ifndef ART_H
#define ART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/queue.h>

#define NODE4   1
#define NODE16  2
#define NODE48  3
#define NODE256 4

#define MAX_PREFIX_LEN 10

typedef int(*art_callback)(void *data,
	const unsigned char *key, uint32_t key_len,
	void *value, uint32_t val_len);

typedef struct _art_leaf art_leaf;

POBJ_LAYOUT_BEGIN(arttree_tx);
POBJ_LAYOUT_ROOT(arttree_tx, struct pmem_art_tree_root);
POBJ_LAYOUT_TOID(arttree_tx, art_leaf);
POBJ_LAYOUT_END(arttree_tx);

struct pmem_art_tree_root {
	POBJ_LIST_HEAD(leafs, art_leaf) qhead;
};


/*
 * This struct is included as part
 * of all the various node sizes
 */
typedef struct {
    uint8_t type;
    uint8_t num_children;
    uint32_t partial_len;
    unsigned char partial[MAX_PREFIX_LEN];
} art_node;

/*
 * Small node with only 4 children
 */
typedef struct {
    art_node n;
    unsigned char keys[4];
    art_node *children[4];
} art_node4;

/*
 * Node with 16 children
 */
typedef struct {
    art_node n;
    unsigned char keys[16];
    art_node *children[16];
} art_node16;

/*
 * Node with 48 children, but
 * a full 256 byte field.
 */
typedef struct {
    art_node n;
    unsigned char keys[256];
    art_node *children[48];
} art_node48;

/*
 * Full node with 256 children
 */
typedef struct {
    art_node n;
    art_node *children[256];
} art_node256;

/*
 * Represents a leaf. These are
 * of arbitrary size, as they include the key.
 */
struct _art_leaf {
    uint32_t key_len;
    uint32_t val_len;
    POBJ_LIST_ENTRY(art_leaf) entries;
    unsigned char buffer[0];
};

/*
 * Main struct, points to root.
 */
typedef struct {
    art_node *root;
    uint64_t size;
} art_tree;

typedef struct _cb_data {
	art_node *node;
	int child_idx;
} cb_data;

/*
 * Initializes an ART tree
 * @return 0 on success.
 */
int art_tree_init(art_tree **t);

/*
 * DEPRECATED
 * Initializes an ART tree
 * @return 0 on success.
 */
#define init_art_tree(...) art_tree_init(__VA_ARGS__)

/*
 * Destroys an ART tree
 * @return 0 on success.
 */
int art_tree_destroy(PMEMobjpool *pop, art_tree *t);

/*
 * DEPRECATED
 * Initializes an ART tree
 * @return 0 on success.
 */
#define destroy_art_tree(...) art_tree_destroy(__VA_ARGS__)

/*
 * Returns the size of the ART tree.
 */
uint64_t art_size(art_tree *t);
inline uint64_t art_size(art_tree *t) {
    return t->size;
}

int art_rebuild_tree_from_pmem_list(PMEMobjpool *pop, art_tree *t);
void *art_insert_leaf(PMEMobjpool *pop, art_tree *t,
	TOID(art_leaf) leaf);

/*
 * Inserts a new value into the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @arg value Opaque value.
 * @return NULL if the item was newly inserted, otherwise
 * the old value pointer is returned.
 */
void *art_insert(PMEMobjpool *pop, art_tree *t,
	const unsigned char *key, int key_len,
	void *value, int val_len);

/*
 * Deletes a value from the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
void *art_delete(PMEMobjpool *pop, art_tree *t,
	const unsigned char *key, int key_len);

/*
 * Searches for a value in the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
void *art_search(PMEMobjpool *pop, const art_tree *t,
	const unsigned char *key, int key_len);

/*
 * Returns the minimum valued leaf
 * @return The minimum leaf or NULL
 */
art_leaf *art_minimum(PMEMobjpool *pop, art_tree *t);

/*
 * Returns the maximum valued leaf
 * @return The maximum leaf or NULL
 */
art_leaf *art_maximum(PMEMobjpool *pop, art_tree *t);

int art_iter_list(PMEMobjpool *pop, art_callback cb, void *data);

/*
 * Iterates through the entries pairs in the map,
 * invoking a callback for each. The call back gets a
 * key, value for each and returns an integer stop value.
 * If the callback returns non-zero, then the iteration stops.
 * @arg t The tree to iterate over
 * @arg cb The callback function to invoke
 * @arg data Opaque handle passed to the callback
 * @return 0 on success, or the return of the callback.
 */
int art_iter(PMEMobjpool *pop, art_tree *t, art_callback cb, void *data);
int art_iter2(PMEMobjpool *pop, art_tree *t, art_callback cb, void *data);

/*
 * Iterates through the entries pairs in the map,
 * invoking a callback for each that matches a given prefix.
 * The call back gets a key, value for each and returns an integer stop value.
 * If the callback returns non-zero, then the iteration stops.
 * @arg t The tree to iterate over
 * @arg prefix The prefix of keys to read
 * @arg prefix_len The length of the prefix
 * @arg cb The callback function to invoke
 * @arg data Opaque handle passed to the callback
 * @return 0 on success, or the return of the callback.
 */
int art_iter_prefix(PMEMobjpool *pop, art_tree *t,
	const unsigned char *prefix, int prefix_len,
	art_callback cb, void *data);

#ifdef __cplusplus
}
#endif

#endif
