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

/*
 * ringbuf.h -- internal definitions for mpmc ring buffer
 */

#ifndef LIBPMEMOBJ_RINGBUF_H
#define LIBPMEMOBJ_RINGBUF_H 1

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

struct ringbuf;

struct ringbuf *ringbuf_new(unsigned length);
void ringbuf_delete(struct ringbuf *rbuf);
unsigned ringbuf_length(struct ringbuf *rbuf);
void ringbuf_stop(struct ringbuf *rbuf);

int ringbuf_enqueue(struct ringbuf *rbuf, void *data);
int ringbuf_tryenqueue(struct ringbuf *rbuf, void *data);
void *ringbuf_dequeue(struct ringbuf *rbuf);
void *ringbuf_trydequeue(struct ringbuf *rbuf);
void *ringbuf_dequeue_s(struct ringbuf *rbuf, size_t data_size);
void *ringbuf_trydequeue_s(struct ringbuf *rbuf, size_t data_size);

int ringbuf_full(struct ringbuf *rbuf);
int ringbuf_empty(struct ringbuf *rbuf);

#endif
