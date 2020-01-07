// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */
/*
 * scenario.hpp -- scenario module declaration
 */

#include "queue.h"
#include <cstdbool>

struct kv {
	PMDK_TAILQ_ENTRY(kv) next;
	char *key;
	char *value;
};

struct scenario {
	PMDK_TAILQ_ENTRY(scenario) next;
	PMDK_TAILQ_HEAD(scenariohead, kv) head;
	char *name;
	char *benchmark;
	char *group;
};

struct scenarios {
	PMDK_TAILQ_HEAD(scenarioshead, scenario) head;
};

#define FOREACH_SCENARIO(s, ss) PMDK_TAILQ_FOREACH((s), &(ss)->head, next)
#define FOREACH_KV(kv, s) PMDK_TAILQ_FOREACH((kv), &(s)->head, next)

struct kv *kv_alloc(const char *key, const char *value);
void kv_free(struct kv *kv);

struct scenario *scenario_alloc(const char *name, const char *bench);
void scenario_free(struct scenario *s);
void scenario_set_group(struct scenario *s, const char *group);

struct scenarios *scenarios_alloc(void);
void scenarios_free(struct scenarios *scenarios);

struct scenario *scenarios_get_scenario(struct scenarios *ss, const char *name);

bool contains_scenarios(int argc, char **argv, struct scenarios *ss);
struct scenario *clone_scenario(struct scenario *src_scenario);
struct kv *find_kv_in_scenario(const char *key,
			       const struct scenario *scenario);
