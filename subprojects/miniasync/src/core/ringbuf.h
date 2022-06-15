/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2022, Intel Corporation */

/*
 * ringbuf.h -- internal definitions for mpmc ring buffer
 */

#ifndef RINGBUF_H
#define RINGBUF_H 1

#include "stddef.h"
#include "stdint.h"

#ifdef _WIN32
#include "windows/include/unistd.h"
#else
#include "unistd.h"
#endif

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

#endif
