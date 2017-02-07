/*
 * Copyright 2016-2017, Intel Corporation
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
 * rpmemd_config.c -- rpmemd config source file
 */

#include <pwd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <inttypes.h>

#include "rpmemd.h"
#include "rpmemd_log.h"
#include "rpmemd_config.h"

#define CONFIG_LINE_SIZE_INIT	50
#define INVALID_CHAR_POS	UINT64_MAX

struct rpmemd_special_chars_pos {
	uint64_t equal_char;
	uint64_t comment_char;
	uint64_t EOL_char;
};

enum rpmemd_option {
	RPD_OPT_LOG_FILE,
	RPD_OPT_POOLSET_DIR,
	RPD_OPT_PERSIST_APM,
	RPD_OPT_PERSIST_GENERAL,
	RPD_OPT_USE_SYSLOG,
	RPD_OPT_LOG_LEVEL,
	RPD_OPT_RM_POOLSET,

	RPD_OPT_MAX_VALUE,
	RPD_OPT_INVALID			= UINT64_MAX,
};

static const char *optstr = "c:hVr:fs";

/*
 * options -- cl and config file options
 */
static const struct option options[] = {
{"config",		required_argument,	0, 'c'},
{"help",		no_argument,		0, 'h'},
{"version",		no_argument,		0, 'V'},
{"log-file",		required_argument,	0, RPD_OPT_LOG_FILE},
{"poolset-dir",		required_argument,	0, RPD_OPT_POOLSET_DIR},
{"persist-apm",		no_argument,		0, RPD_OPT_PERSIST_APM},
{"persist-general",	no_argument,		0, RPD_OPT_PERSIST_GENERAL},
{"use-syslog",		no_argument,		0, RPD_OPT_USE_SYSLOG},
{"log-level",		required_argument,	0, RPD_OPT_LOG_LEVEL},
{"remove",		required_argument,	0, 'r'},
{"force",		no_argument,		0, 'f'},
{"pool-set",		no_argument,		0, 's'},
{0,			0,			0,  0},
};

#define VALUE_INDENT	"                                        "

static const char *help_str =
"\n"
"Options:\n"
"  -c, --config <path>           configuration file location\n"
"  -r, --remove <poolset>        remove pool described by given poolset file\n"
"  -f, --force                   ignore errors when removing a pool\n"
"  -h, --help                    display help message and exit\n"
"  -V, --version                 display target daemon version and exit\n"
"      --log-file <path>         log file location\n"
"      --poolset-dir <path>      pool set files directory\n"
"      --persist-apm             enable Appliance Persistency Method\n"
"      --persist-general         enable General Server Persistency Mechanism\n"
"      --use-syslog              use syslog(3) for logging messages\n"
"      --log-level <level>       set log level value\n"
VALUE_INDENT "err     error conditions\n"
VALUE_INDENT "warn    warning conditions\n"
VALUE_INDENT "notice  normal, but significant, condition\n"
VALUE_INDENT "info    informational message\n"
VALUE_INDENT "debug   debug-level message\n"
"\n"
"For complete documentation see %s(1) manual page.";

/*
 * print_version -- (internal) prints version message
 */
static void
print_version(void)
{
	RPMEMD_LOG(ERR, "%s version %s", DAEMON_NAME, SRCVERSION);
}

/*
 * print_usage -- (internal) prints usage message
 */
static void
print_usage(const char *name)
{
	RPMEMD_LOG(ERR, "usage: %s [--version] [--help] [<args>]",
		name);
}

/*
 * print_help -- (internal) prints help message
 */
static void
print_help(const char *name)
{
	print_usage(name);
	print_version();
	RPMEMD_LOG(ERR, help_str, DAEMON_NAME);
}

/*
 * parse_config_string -- (internal) parse string value
 */
static inline char *
parse_config_string(const char *value)
{
	if (strlen(value) == 0) {
		errno = EINVAL;
		return NULL;
	}
	char *output = strdup(value);
	if (output == NULL)
		RPMEMD_FATAL("!strdup");
	return output;
}

/*
 * parse_config_bool -- (internal) parse yes / no flag
 */
static inline void
parse_config_bool(bool *config_value, const char *value)
{
	if (value == NULL)
		*config_value = true;
	else if (strcmp("yes", value) == 0)
		*config_value = true;
	else if (strcmp("no", value) == 0)
		*config_value = false;
	else
		errno = EINVAL;
}

/*
 * set_option -- (internal) set single config option
 */
static int
set_option(enum rpmemd_option option, const char *value,
	struct rpmemd_config *config)
{
	errno = 0;

	switch (option) {
	case RPD_OPT_LOG_FILE:
		free(config->log_file);
		config->log_file = parse_config_string(value);
		config->use_syslog = false;
		break;
	case RPD_OPT_POOLSET_DIR:
		free(config->poolset_dir);
		config->poolset_dir = parse_config_string(value);
		break;
	case RPD_OPT_PERSIST_APM:
		parse_config_bool(&config->persist_apm, value);
		break;
	case RPD_OPT_PERSIST_GENERAL:
		parse_config_bool(&config->persist_general, value);
		break;
	case RPD_OPT_USE_SYSLOG:
		parse_config_bool(&config->use_syslog, value);
		break;
	case RPD_OPT_LOG_LEVEL:
		config->log_level = rpmemd_log_level_from_str(value);
		if (config->log_level == MAX_RPD_LOG)
			errno = EINVAL;
		break;
	default:
		errno = EINVAL;
	}

	if (errno != 0)
		return 1;
	else
		return 0;
}

/*
 * get_config_line -- (internal) read single line from file
 */
static int
get_config_line(FILE *file, char **line, uint64_t *line_max,
	uint8_t *line_max_increased, struct rpmemd_special_chars_pos *pos)
{
	uint8_t line_complete = 0;
	uint64_t line_length = 0;
	char *line_part = *line;
	do {
		char *ret = fgets(line_part,
			(int)(*line_max - line_length), file);
		if (ret == NULL)
			return 0;
		for (uint64_t i = 0; i < *line_max; ++i) {
			if (line_part[i] == '\n')
				line_complete = 1;
			else if (line_part[i] == '\0') {
				line_length += i;
				if (line_length + 1 < *line_max)
					line_complete = 1;
				break;
			} else if (line_part[i] == '#' &&
				pos->comment_char == UINT64_MAX)
				pos->comment_char = line_length + i;
			else if (line_part[i] == '=' &&
				pos->equal_char == UINT64_MAX)
				pos->equal_char = line_length + i;
		}
		if (line_complete == 0) {
			*line = realloc(*line, sizeof(char) * (*line_max) * 2);
			if (*line == NULL) {
				RPMEMD_FATAL("!realloc");
			}
			line_part = *line + *line_max - 1;
			line_length = *line_max - 1;
			*line_max *= 2;
			*line_max_increased = 1;
		}
	} while (line_complete != 1);

	pos->EOL_char = line_length;
	return 0;
}

/*
 * trim_line_element -- (internal) remove white characters
 */
static char *
trim_line_element(char *line, uint64_t start, uint64_t end)
{
	for (; start <= end; ++start) {
		if (!isspace(line[start]))
			break;
	}

	for (; end > start; --end) {
		if (!isspace(line[end - 1]))
			break;
	}

	if (start == end)
		return NULL;

	line[end] = '\0';
	return &line[start];
}

/*
 * parse_config_key -- (internal) lookup config key
 */
static enum rpmemd_option
parse_config_key(const char *key)
{
	for (int i = 0; options[i].name != 0; ++i) {
		if (strcmp(key, options[i].name) == 0)
			return (enum rpmemd_option)options[i].val;
	}

	return RPD_OPT_INVALID;
}

/*
 * parse_config_line -- (internal) parse single config line
 *
 * Return newly written option flag. Store possible errors in errno.
 */
static void
parse_config_line(char *line, struct rpmemd_special_chars_pos *pos,
	struct rpmemd_config *config, uint64_t disabled)
{
	if (pos->comment_char < pos->equal_char)
		pos->equal_char = INVALID_CHAR_POS;

	uint64_t end_of_content = pos->comment_char != INVALID_CHAR_POS ?
		pos->comment_char : pos->EOL_char;

	if (pos->equal_char == INVALID_CHAR_POS) {
		char *leftover = trim_line_element(line, 0, end_of_content);
		if (leftover != NULL)
			errno = EINVAL;
		return;
	}

	char *key_name = trim_line_element(line, 0, pos->equal_char);
	char *value = trim_line_element(line, pos->equal_char + 1,
		end_of_content);

	if (key_name == NULL || value == NULL) {
		errno = EINVAL;
		return;
	}

	enum rpmemd_option key = parse_config_key(key_name);
	if (key != RPD_OPT_INVALID) {
		if ((disabled & (uint64_t)(1 << key)) == 0)
			set_option(key, value, config);
	} else
		errno = EINVAL;
}

/*
 * parse_config_file -- (internal) parse config file
 */
static int
parse_config_file(const char *filename, struct rpmemd_config *config,
	uint64_t disabled, int required)
{
	RPMEMD_ASSERT(filename != NULL);

	FILE *file = fopen(filename, "r");
	if (file == NULL) {
		if (required) {
			RPMEMD_LOG(ERR, "!%s", filename);
			goto error_fopen;
		} else {
			goto optional_config_missing;
		}
	}

	uint8_t line_max_increased = 0;
	uint64_t line_max = CONFIG_LINE_SIZE_INIT;
	uint64_t line_num = 1;
	char *line = (char *)malloc(sizeof(char) * line_max);
	if (line == NULL) {
		RPMEMD_LOG(ERR, "!malloc");
		goto error_malloc_line;
	}

	char *line_copy = (char *)malloc(sizeof(char) * line_max);
	if (line_copy == NULL) {
		RPMEMD_LOG(ERR, "!malloc");
		goto error_malloc_line_copy;
	}

	struct rpmemd_special_chars_pos pos;

	do {
		memset(&pos, 0xff, sizeof(pos));
		if (get_config_line(file, &line, &line_max,
			&line_max_increased, &pos) != 0)
			goto error;

		if (line_max_increased) {
			line_copy = (char *)realloc(line_copy,
				sizeof(char) * line_max);
			if (line_copy == NULL) {
				RPMEMD_LOG(ERR, "!malloc");
				goto error_malloc_line_copy;
			}
			line_max_increased = 0;
		}

		if (pos.EOL_char != INVALID_CHAR_POS) {
			strcpy(line_copy, line);
			parse_config_line(line_copy, &pos, config, disabled);
			if (errno != 0) {
				size_t len = strlen(line);
				if (len > 0 && line[len - 1] == '\n')
					line[len - 1] = '\0';
				RPMEMD_LOG(ERR, "Invalid config file line at "
					"%s:%lu\n%s",
					filename, line_num, line);
				goto error;
			}
		}
		++line_num;
	} while (pos.EOL_char != INVALID_CHAR_POS);

	free(line_copy);
	free(line);
	fclose(file);
optional_config_missing:
	return 0;

error:
	free(line_copy);
error_malloc_line_copy:
	free(line);
error_malloc_line:
	fclose(file);
error_fopen:
	return -1;
}

/*
 * parse_cl_args -- (internal) parse command line arguments
 */
static void
parse_cl_args(int argc, char *argv[], struct rpmemd_config *config,
		const char **config_file, uint64_t *cl_options)
{
	RPMEMD_ASSERT(argv != NULL);
	RPMEMD_ASSERT(config != NULL);

	int opt;
	int option_index = 0;

	while ((opt = getopt_long(argc, argv, optstr, options,
		&option_index)) != -1) {

		switch (opt) {
		case 'c':
			(*config_file) = optarg;
			break;
		case 'r':
			config->rm_poolset = optarg;
			break;
		case 'f':
			config->force = true;
			break;
		case 's':
			config->pool_set = true;
			break;
		case 'h':
			print_help(argv[0]);
			exit(0);
		case 'V':
			print_version();
			exit(0);
			break;
		default:
			if (set_option((enum rpmemd_option)opt, optarg, config)
					== 0) {
				*cl_options |= (uint64_t)(1 << opt);
			} else {
				print_usage(argv[0]);
				exit(-1);
			}
		}
	}
}

/*
 * get_home_dir -- (internal) return user home directory
 *
 * Function will lookup user home directory in order:
 * 1. HOME environment variable
 * 2. Password file entry using real user ID
 */
static void
get_home_dir(char *str, size_t size)
{
	char *home = getenv(HOME_ENV);
	if (home) {
		int r = snprintf(str, size, "%s", home);
		if (r < 0)
			RPMEMD_FATAL("!snprintf");
	} else {
		uid_t uid = getuid();
		struct passwd *pw = getpwuid(uid);
		if (pw == NULL)
			RPMEMD_FATAL("!getpwuid");

		int r = snprintf(str, size, "%s", pw->pw_dir);
		if (r < 0)
			RPMEMD_FATAL("!snprintf");
	}
}

/*
 * concat_dir_and_file_name -- (internal) concatenate directory and file name
 * into single string path
 */
static void
concat_dir_and_file_name(char *path, size_t size, const char *dir,
	const char *file)
{
	int r = snprintf(path, size, "%s/%s", dir, file);
	if (r < 0)
		RPMEMD_FATAL("!snprintf");
}

/*
 * str_replace_home -- (internal) replace $HOME string with user home directory
 *
 * If function does not find $HOME string it will return haystack untouched.
 * Otherwise it will allocate new string with $HOME replaced with provided
 * home_dir path. haystack will be released and newly created string returned.
 */
static char *
str_replace_home(char *haystack, const char *home_dir)
{
	const size_t placeholder_len = strlen(HOME_STR_PLACEHOLDER);
	const size_t home_len = strlen(home_dir);
	size_t haystack_len = strlen(haystack);

	char *pos = strstr(haystack, HOME_STR_PLACEHOLDER);
	if (!pos)
		return haystack;

	const char *after = pos + placeholder_len;
	if (isalnum(*after))
		return haystack;

	haystack_len += home_len - placeholder_len + 1;
	char *buf = malloc(sizeof(char) * haystack_len);
	if (!buf)
		RPMEMD_FATAL("!snprintf");

	*pos = '\0';
	int r = snprintf(buf, haystack_len, "%s%s%s", haystack, home_dir,
		after);
	if (r < 0)
		RPMEMD_FATAL("!snprintf");

	free(haystack);
	return buf;
}

/*
 * config_set_default -- (internal) load default config
 */
static void
config_set_default(struct rpmemd_config *config, const char *poolset_dir)
{
	config->log_file = strdup(RPMEMD_DEFAULT_LOG_FILE);
	if (!config->log_file)
		RPMEMD_FATAL("!strdup");

	config->poolset_dir = strdup(poolset_dir);
	if (!config->poolset_dir)
		RPMEMD_FATAL("!strdup");

	config->persist_apm	= false;
	config->persist_general	= true;
	config->use_syslog	= true;
	config->max_lanes	= RPMEM_DEFAULT_MAX_LANES;
	config->log_level	= RPD_LOG_ERR;
	config->rm_poolset	= NULL;
	config->force		= false;
}

/*
 * rpmemd_config_read -- read config from cl and config files
 *
 * cl param overwrites configuration from any config file. Config file are read
 * in order:
 * 1. Global config file
 * 2. User config file
 * or
 * cl provided config file
 */
int
rpmemd_config_read(struct rpmemd_config *config, int argc, char *argv[])
{
	const char *cl_config_file = NULL;
	char user_config_file[PATH_MAX];
	char home_dir[PATH_MAX];
	uint64_t cl_options = 0;

	get_home_dir(home_dir, PATH_MAX);
	config_set_default(config, home_dir);
	parse_cl_args(argc, argv, config, &cl_config_file, &cl_options);

	if (cl_config_file) {
		if (parse_config_file(cl_config_file, config, cl_options, 1))
			return 1;
	} else {
		if (parse_config_file(RPMEMD_GLOBAL_CONFIG_FILE, config,
				cl_options, 0))
			return 1;

		concat_dir_and_file_name(user_config_file, PATH_MAX, home_dir,
			RPMEMD_USER_CONFIG_FILE);
		if (parse_config_file(user_config_file, config, cl_options, 0))
			return 1;
	}

	config->poolset_dir = str_replace_home(config->poolset_dir, home_dir);
	return 0;
}

/*
 * rpmemd_config_free -- rpmemd config release
 */
void
rpmemd_config_free(struct rpmemd_config *config)
{
	free(config->log_file);
	free(config->poolset_dir);
}
