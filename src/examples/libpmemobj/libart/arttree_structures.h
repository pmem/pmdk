/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */
/*
 * Copyright 2016, FUJITSU TECHNOLOGY SOLUTIONS GMBH
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
 *       Filename:  arttree_structures.h
 *
 *    Description:  known structures of the ART tree
 *
 *         Author:  Andreas Bluemle, Dieter Kasper
 *                  Andreas.Bluemle.external@ts.fujitsu.com
 *                  dieter.kasper@ts.fujitsu.com
 *
 *   Organization:  FUJITSU TECHNOLOGY SOLUTIONS GMBH
 *
 * ===========================================================================
 */
#ifndef _ARTTREE_STRUCTURES_H
#define _ARTTREE_STRUCTURES_H

#define MAX_PREFIX_LEN 10

/*
 * pmem_context -- structure for pmempool file
 */
struct pmem_context {
    char *filename;
    size_t psize;
    int fd;
    char *addr;
    uint64_t art_tree_root_offset;
};

struct _art_node_u;	typedef struct _art_node_u	art_node_u;
struct _art_node;	typedef struct _art_node	art_node;
struct _art_node4;	typedef struct _art_node4	art_node4;
struct _art_node16;	typedef struct _art_node16	art_node16;
struct _art_node48;	typedef struct _art_node48	art_node48;
struct _art_node256;	typedef struct _art_node256	art_node256;
struct _var_string;	typedef struct _var_string	var_string;
struct _art_leaf;	typedef struct _art_leaf	art_leaf;
struct _art_tree_root;	typedef struct _art_tree_root	art_tree_root;

typedef uint8_t art_tree_root_toid_type_num[65535];
typedef uint8_t _toid_art_node_u_toid_type_num[2];
typedef uint8_t _toid_art_node_toid_type_num[3];
typedef uint8_t _toid_art_node4_toid_type_num[4];
typedef uint8_t _toid_art_node16_toid_type_num[5];
typedef uint8_t _toid_art_node48_toid_type_num[6];
typedef uint8_t _toid_art_node256_toid_type_num[7];
typedef uint8_t _toid_art_leaf_toid_type_num[8];
typedef uint8_t _toid_var_string_toid_type_num[9];

typedef struct pmemoid {
	uint64_t pool_uuid_lo;
	uint64_t off;
} PMEMoid;

union _toid_art_node_u_toid {
	PMEMoid oid;
	art_node_u *_type;
	_toid_art_node_u_toid_type_num *_type_num;
};

union art_tree_root_toid {
	PMEMoid oid;
	struct art_tree_root *_type;
	art_tree_root_toid_type_num *_type_num;
};

union _toid_art_node_toid {
	PMEMoid oid;
	art_node *_type;
	_toid_art_node_toid_type_num *_type_num;
};

union _toid_art_node4_toid {
	PMEMoid oid;
	art_node4 *_type;
	_toid_art_node4_toid_type_num *_type_num;
};

union _toid_art_node16_toid {
	PMEMoid oid;
	art_node16 *_type;
	_toid_art_node16_toid_type_num *_type_num;
};

union _toid_art_node48_toid {
	PMEMoid oid;
	art_node48 *_type;
	_toid_art_node48_toid_type_num *_type_num;
};

union _toid_art_node256_toid {
	PMEMoid oid;
	art_node256 *_type;
	_toid_art_node256_toid_type_num *_type_num;
};

union _toid_var_string_toid {
	PMEMoid oid;
	var_string *_type;
	_toid_var_string_toid_type_num *_type_num;
};

union _toid_art_leaf_toid {
	PMEMoid oid;
	art_leaf *_type;
	_toid_art_leaf_toid_type_num *_type_num;
};

struct _art_tree_root {
	int size;
	union _toid_art_node_u_toid root;
};

struct _art_node {
	uint8_t num_children;
	uint32_t partial_len;
	unsigned char partial[MAX_PREFIX_LEN];
};

struct _art_node4 {
	art_node n;
	unsigned char keys[4];
	union _toid_art_node_u_toid children[4];
};

struct _art_node16 {
	art_node n;
	unsigned char keys[16];
	union _toid_art_node_u_toid children[16];
};

struct _art_node48 {
	art_node n;
	unsigned char keys[256];
	union _toid_art_node_u_toid children[48];
};

struct _art_node256 {
	art_node n;
	union _toid_art_node_u_toid children[256];
};

struct _var_string {
	size_t len;
	unsigned char s[];
};

struct _art_leaf {
	union _toid_var_string_toid value;
	union _toid_var_string_toid key;
};

struct _art_node_u {
	uint8_t art_node_type;
	uint8_t art_node_tag;
	union {
		union _toid_art_node4_toid an4;
		union _toid_art_node16_toid an16;
		union _toid_art_node48_toid an48;
		union _toid_art_node256_toid an256;
		union _toid_art_leaf_toid al;
	} u;
};

typedef enum {
	ART_NODE4	= 0,
	ART_NODE16	= 1,
	ART_NODE48	= 2,
	ART_NODE256	= 3,
	ART_LEAF	= 4,
	ART_NODE_U	= 5,
	ART_NODE	= 6,
	ART_TREE_ROOT	= 7,
	VAR_STRING	= 8,
	art_node_types	= 9   /* number of different art_nodes */
} art_node_type;

#define VALID_NODE_TYPE(n) (((n) >= 0) && ((n) < art_node_types))

extern size_t art_node_sizes[];
extern char *art_node_names[];

#endif /* _ARTTREE_STRUCTURES_H */
