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
 * rpmemd_fip_ring.h -- single producer single consumer ring buffer
 */

#include <stddef.h>
#include <stdlib.h>

/*
 * rpmemd_fip_ring -- single producer single consumer ring buffer struct
 */
struct rpmemd_fip_ring {
	size_t nslots;		/* number of slots */
	volatile size_t head;	/* ring buffer head index */
	volatile size_t tail;	/* ring buffer tail index */
	void *data[];		/* buffer */
};

/*
 * rpmemd_fip_ring_alloc -- allocate ring buffer instance
 */
static inline struct rpmemd_fip_ring *
rpmemd_fip_ring_alloc(size_t size)
{
	size_t nslots = size + 1;
	struct rpmemd_fip_ring *ring = malloc(sizeof(*ring) +
			nslots * sizeof(void *));
	if (!ring)
		return NULL;

	ring->nslots = nslots;
	ring->head = 0;
	ring->tail = 0;
	return ring;
}

/*
 * rpmemd_fip_ring_free -- deallocate ring buffer instance
 */
static void
rpmemd_fip_ring_free(struct rpmemd_fip_ring *ring)
{
	free(ring);
}

/*
 * rpmemd_fip_ring_is_full -- returns true if buffer is full
 */
static inline int
rpmemd_fip_ring_is_full(struct rpmemd_fip_ring *ring)
{
	return (ring->tail + 1) % ring->nslots == ring->head;
}

/*
 * rpmemd_fip_ring_is_empty -- returns true if buffer is empty
 */
static inline int
rpmemd_fip_ring_is_empty(struct rpmemd_fip_ring *ring)
{
	return ring->head == ring->tail;
}

/*
 * rpmemd_fip_ring_push -- push an element to the ring buffer
 */
static inline int
rpmemd_fip_ring_push(struct rpmemd_fip_ring *ring, void *data)
{
	if (rpmemd_fip_ring_is_full(ring))
		return -1;

	ring->data[ring->tail] = data;
	ring->tail = (ring->tail + 1) % ring->nslots;

	return 0;
}

/*
 * rpmemd_fip_ring_pop -- pop an element from the ring buffer
 */
static inline void *
rpmemd_fip_ring_pop(struct rpmemd_fip_ring *ring)
{
	if (rpmemd_fip_ring_is_empty(ring))
		return NULL;

	void *ret = ring->data[ring->head];

	ring->head = (ring->head + 1) % ring->nslots;

	return ret;
}
