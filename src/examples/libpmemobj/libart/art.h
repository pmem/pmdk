/*
 * Copyright 2016, FUJITSU TECHNOLOGY SOLUTIONS GMBH
 * Copyright 2012, Armon Dadgar. All rights reserved.
 * Copyright 2019, Intel Corporation
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
 *    Description:  header file for art tree on pmem implementation
 *
 *         Author:  Andreas Bluemle, Dieter Kasper
 *                  Andreas.Bluemle.external@ts.fujitsu.com
 *                  dieter.kasper@ts.fujitsu.com
 *
 *   Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
 *
 * ===========================================================================
 */

/*
 * based on https://github.com/armon/libart/src/art.h
 */

#ifndef _ART_H
#define _ART_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PREFIX_LEN 10

typedef enum {
	NODE4		= 0,
	NODE16		= 1,
	NODE48		= 2,
	NODE256		= 3,
	art_leaf_t	= 4,
	art_node_types	= 5   /* number of different art_nodes */
} art_node_type;

char *art_node_names[] = {
	"art_node4",
	"art_node16",
	"art_node48",
	"art_node256",
	"art_leaf"
};

/*
 * forward declarations; these are required when typedef shall be
 * used instead of struct
 */
struct _art_node_u;	typedef struct _art_node_u art_node_u;
struct _art_node;	typedef struct _art_node art_node;
struct _art_node4;	typedef struct _art_node4 art_node4;
struct _art_node16;	typedef struct _art_node16 art_node16;
struct _art_node48;	typedef struct _art_node48 art_node48;
struct _art_node256;	typedef struct _art_node256 art_node256;
struct _art_leaf;	typedef struct _art_leaf art_leaf;
struct _var_string;	typedef struct _var_string var_string;

POBJ_LAYOUT_BEGIN(arttree_tx);
POBJ_LAYOUT_ROOT(arttree_tx, struct art_tree_root);
POBJ_LAYOUT_TOID(arttree_tx, art_node_u);
POBJ_LAYOUT_TOID(arttree_tx, art_node4);
POBJ_LAYOUT_TOID(arttree_tx, art_node16);
POBJ_LAYOUT_TOID(arttree_tx, art_node48);
POBJ_LAYOUT_TOID(arttree_tx, art_node256);
POBJ_LAYOUT_TOID(arttree_tx, art_leaf);
POBJ_LAYOUT_TOID(arttree_tx, var_string);
POBJ_LAYOUT_END(arttree_tx);

struct _var_string {
	size_t len;
	unsigned char s[];
};

/*
 * This struct is included as part of all the various node sizes
 */
struct _art_node {
	uint8_t num_children;
	uint32_t partial_len;
	unsigned char partial[MAX_PREFIX_LEN];
};

/*
 * Small node with only 4 children
 */
struct _art_node4 {
	art_node n;
	unsigned char keys[4];
	TOID(art_node_u) children[4];
};

/*
 * Node with 16 children
 */
struct _art_node16 {
	art_node n;
	unsigned char keys[16];
	TOID(art_node_u) children[16];
};

/*
 * Node with 48 children, but a full 256 byte field.
 */
struct _art_node48 {
	art_node n;
	unsigned char keys[256];
	TOID(art_node_u) children[48];
};

/*
 * Full node with 256 children
 */
struct _art_node256 {
	art_node n;
	TOID(art_node_u) children[256];
};

/*
 * Represents a leaf. These are of arbitrary size, as they include the key.
 */
struct _art_leaf {
	TOID(var_string) value;
	TOID(var_string) key;
};

struct _art_node_u {
	uint8_t art_node_type;
	uint8_t art_node_tag;
	union {
		TOID(art_node4)   an4;   /*  starts with art_node */
		TOID(art_node16)  an16;  /*  starts with art_node */
		TOID(art_node48)  an48;  /*  starts with art_node */
		TOID(art_node256) an256; /*  starts with art_node */
		TOID(art_leaf)    al;
	} u;
};

struct art_tree_root {
	int size;
	TOID(art_node_u) root;
};

typedef struct _cb_data {
	TOID(art_node_u) node;
	int child_idx;
} cb_data;

/*
 * Macros to manipulate art_node tags
 */
#define IS_LEAF(x) (((x)->art_node_type == art_leaf_t))
#define SET_LEAF(x) (((x)->art_node_tag = art_leaf_t))

#define COPY_BLOB(_obj, _blob, _len) \
    D_RW(_obj)->len = _len; \
    TX_MEMCPY(D_RW(_obj)->s, _blob, _len); \
    D_RW(_obj)->s[(_len) - 1] = '\0';

typedef int(*art_callback)(void *data,
		const unsigned char *key, uint32_t key_len,
		const unsigned char *value, uint32_t val_len);

extern int art_tree_init(PMEMobjpool *pop, int *newpool);
extern uint64_t art_size(PMEMobjpool *pop);
extern int art_iter(PMEMobjpool *pop, art_callback cb, void *data);
extern TOID(var_string) art_insert(PMEMobjpool *pop,
		const unsigned char *key, int key_len,
		void *value, int val_len);
extern TOID(var_string) art_search(PMEMobjpool *pop,
		const unsigned char *key, int key_len);
extern TOID(var_string) art_delete(PMEMobjpool *pop,
		const unsigned char *key, int key_len);

#ifdef __cplusplus
}
#endif

#endif /* _ART_H */
