#include <ctype.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "conf.h"
#include "list.h"
#include "log.h"
#include "song_manager.h"

#define PROGRAM_DIR_NAME "mpd-auto-queue"
#define LEN(x) sizeof(x) / sizeof(*x)
/* clang-format off */
#define CONF_STR (struct conf_str){NULL, 0, 2}
/* clang-format on */

enum CONFIG_STATE {
	COMMENT,
	KEY,
	VALUE,
};

struct conf_str {
	char *str;
	int str_length;
	size_t size;
};

enum CONF_TYPE {
	CONF_TYPE_INT,
	CONF_TYPE_STR,
	CONF_TYPE_BOOL,
	CONF_TYPE_QUEUE_METHOD,
	CONF_TYPE_QUEUE_WEIGHT,
};

struct conf_item {
	const char *name;
	enum CONF_TYPE type;
	void *data;
};

static struct config conf = {6600, NULL, NULL, 1, NEW_LIST, NULL, 5, 10, NULL};

static const struct conf_item conf_items[] = {
    {"mpd_host",             CONF_TYPE_STR,          &conf.mpd_host         },
    {"mpd_port",             CONF_TYPE_INT,          &conf.mpd_port         },
    {"mpd_password",         CONF_TYPE_STR,          &conf.mpd_password     },
    {"lastfm_api_key",       CONF_TYPE_STR,          &conf.lastfm_api_key   },
    {"use_cache",            CONF_TYPE_BOOL,         &conf.use_cache        },
    {"min_songs_left",       CONF_TYPE_INT,          &conf.min_songs_left   },
    {"auto_queue_amount",    CONF_TYPE_INT,          &conf.auto_queue_amount},
    {"queue_methods",        CONF_TYPE_QUEUE_METHOD, &conf.queue_methods    },
    {"queue_method_weights", CONF_TYPE_QUEUE_WEIGHT, &conf.queue_methods    },
};

static char *
get_dir(const char *env_var, const char *default_path) {
	const char *path, *home_path;
	char *dir;

	if ((path = getenv(env_var)) != NULL) {
		dir = malloc(strlen(path) + strlen("/" PROGRAM_DIR_NAME "/"));
		strcpy(dir, path);
		strcat(dir, "/" PROGRAM_DIR_NAME "/");
	} else {
		if ((home_path = getenv("HOME")) == NULL)
			home_path = getpwuid(getuid())->pw_dir;
		dir = malloc(strlen(home_path) + strlen(default_path) + 1);
		strcpy(dir, home_path);
		strcat(dir, default_path);
	}
	mkdir(dir, 0777);

	return dir;
}

static char *
get_cache_dir() {
	return get_dir("XDG_CACHE_HOME", "/.cache/" PROGRAM_DIR_NAME "/");
}

static FILE *
get_config_file() {
	FILE *conf_file;
	char *config_dir =
	    get_dir("XDG_CONFIG_HOME", "/.config/" PROGRAM_DIR_NAME "/");

	config_dir =
	    realloc(config_dir, strlen(config_dir) +
	                            strlen("/" PROGRAM_DIR_NAME ".conf") + 1);

	strcat(config_dir, "/" PROGRAM_DIR_NAME ".conf");

	conf_file = fopen(config_dir, "r");
	free(config_dir);
	return conf_file;
}

static void
conf_str_append(struct conf_str *cstr, char c) {
	cstr->str_length++;
	if ((size_t)(cstr->str_length + 1) >= cstr->size) {
		cstr->size *= 2;
		cstr->str = realloc(cstr->str, cstr->size);
	}
	cstr->str[cstr->str_length - 1] = c;
	cstr->str[cstr->str_length] = '\0';
}

static size_t
trim_whitespace(char *orig_str) {
	size_t spaces_encountered = 0;
	size_t len = 0;
	const char *str = orig_str;

	if (orig_str == NULL)
		return -1;

	for (; *str == ' '; str++)
		/* pass */;

	for (; *str != '\0'; str++) {
		if (*str == ' ') {
			if (!spaces_encountered) {
				orig_str[len] = *str;
				len++;
			}
			spaces_encountered = 1;
		} else if (!isspace(*str) && !isblank(*str)) {
			orig_str[len] = *str;
			len++;
			spaces_encountered = 0;
		}
	}

	if (spaces_encountered)
		len--;

	orig_str[len] = '\0';
	return len;
}

static int
conf_str_to_int(const char *value, const char *key, int line_number) {
	const char *str = value;

	if (value == NULL || value[0] == '\0')
		return -1;

	if (*str == '-') {
		log_data(LOG_ERROR,
		         "Value \"%s\" used with \"%s\" at "
		         "line %d cannot be negative,",
		         value, key, line_number);
		return -1;
	}

	for (; *str != '\0'; str++) {
		if (!isdigit(*str)) {
			log_data(LOG_ERROR,
			         "Value \"%s\" used with \"%s\" at "
			         "line %d is not a number,",
			         value, key, line_number);
			return -1;
		}
	}

	return atoi(value);
}

const char *
conf_get_next_array_item(struct conf_str *cstr, int line_number) {
	static struct conf_str *cstr_array = NULL;
	static int pos = 0;
	char *return_str;

	if (pos < 0) {
		pos = 0;
		return NULL;
	}

	if (cstr != NULL && cstr_array != cstr) {
		cstr_array = cstr;
		pos = 0;
	}

	if (cstr_array == NULL) {
		return NULL;
	}

	if (pos == 0 && cstr_array->str[0] != '[') {
		log_data(LOG_ERROR, "Value \"%s\" at line %d is not a list",
		         cstr_array->str, line_number);
		return NULL;
	}

	if (cstr_array->str[pos] == ']') {
		cstr_array = NULL;
		return NULL;
	}

	pos++;

	return_str = &cstr_array->str[pos];
	for (; cstr_array->str[pos] != ','; pos++) {
		if (cstr_array->str[pos] == ']') {
			cstr_array->str[pos] = '\0';
			trim_whitespace(return_str);
			cstr_array = NULL;
			pos = -1;
			return return_str;
			break;
		} else if (cstr_array->str[pos] == '\0') {
			log_data(
			    LOG_ERROR,
			    "List at line %d does not have an ending bracket",
			    line_number);
			return NULL;
		}
	}

	cstr_array->str[pos] = '\0';
	trim_whitespace(return_str);
	pos++;

	return return_str;
}

static void
conf_str_set_value(struct conf_str *key, struct conf_str *value,
                   int line_number) {
	int temp;
	size_t i, j;
	const char *temp_str;
	struct queue_method *method;
	void *item_data;

	key->str_length = trim_whitespace(key->str);
	value->str_length = trim_whitespace(value->str);

	if (key->str_length <= 0) {
		log_data(LOG_ERROR, "Key at line %d cannot be blank",
		         line_number);
		goto end;
	}
	if (value->str_length <= 0) {
		log_data(LOG_ERROR,
		         "Value used with \"%s\" at line %d cannot be blank",
		         key->str, line_number);
		goto end;
	}

	for (i = 0; i < LEN(conf_items); i++) {
		if (!strcmp(key->str, conf_items[i].name)) {
			item_data = conf_items[i].data;

			switch (conf_items[i].type) {
			case CONF_TYPE_STR:
				*((char **)item_data) =
				    realloc(value->str, value->str_length + 1);
				value->str = NULL;
				log_data(LOG_VERBOSE, "%s set to \"%s\"",
				         conf_items[i].name,
				         *((char **)item_data));
				break;
			case CONF_TYPE_INT:
				temp = conf_str_to_int(value->str, key->str,
				                       line_number);
				if (temp != -1)
					*((int *)item_data) = temp;
				log_data(LOG_VERBOSE, "%s set to %d",
				         conf_items[i].name,
				         *((int *)item_data));
				break;
			case CONF_TYPE_BOOL:
				if (value->str[0] == 't' ||
				    value->str[0] == 'y' ||
				    value->str[0] == '1')
					*((int *)item_data) = 1;
				else
					*((int *)item_data) = 0;
				log_data(LOG_VERBOSE, "%s set to %d",
				         conf_items[i].name,
				         *((int *)item_data));

				break;
			case CONF_TYPE_QUEUE_METHOD:
				for (j = 0;
				     (temp_str = conf_get_next_array_item(
				          value, line_number)) != NULL;
				     j++) {
					if (((List *)conf_items[i].data)
					        ->length >= j + 1) {
						method =
						    ((List *)conf_items[i].data)
						        ->items[j];
					} else {
						method =
						    malloc(sizeof(*method));
						*method = (struct queue_method){
						    QM_UNDEFINED, -1};
					}

					if (!strcmp(temp_str, "same_artist")) {
						method->type = QM_SAME_ARTIST;
					} else if (!strcmp(temp_str,
					                   "related_artist")) {
						method->type =
						    QM_RELATED_ARTIST;
					} else if (!strcmp(temp_str,
					                   "random")) {
						method->type = QM_RANDOM;
					} else {
						log_data(LOG_ERROR,
						         "Unknown queue method "
						         "\"%s\" at line %d",
						         temp_str, line_number);
						free(method);
						j--;
						continue;
					}
					append_to_list(conf_items[i].data,
					               method);
					log_data(LOG_VERBOSE,
					         "Adding method type \"%s\"",
					         temp_str);
				}
				break;
			case CONF_TYPE_QUEUE_WEIGHT:
				for (j = 0;
				     (temp_str = conf_get_next_array_item(
				          value, line_number)) != NULL;
				     j++) {
					temp = conf_str_to_int(
					    temp_str, key->str, line_number);
					if (temp < 0) {
						j--;
						continue;
					}

					if (((List *)conf_items[i].data)
					        ->length >= j + 1) {
						method =
						    ((List *)conf_items[i].data)
						        ->items[j];
					} else {
						method =
						    malloc(sizeof(*method));
						*method = (struct queue_method){
						    QM_UNDEFINED, -1};
						append_to_list(
						    conf_items[i].data, method);
					}

					method->weight = temp;
					log_data(LOG_VERBOSE,
					         "Adding method weight %d",
					         method->weight);
				}
				break;
			}
		}
	}
end:
	key->str_length = 0;
	if (key->str != NULL)
		key->str[0] = '\0';

	if (value->str == NULL) {
		free(value->str);
		*value = CONF_STR;
	} else {
		value->str_length = 0;
		value->str[0] = '\0';
	}
}

static void
parse_config(FILE *config_file) {
	char buf[4096];
	size_t bytes_read, i;
	int line_number = 1;

	struct conf_str key = CONF_STR;
	struct conf_str value = CONF_STR;

	enum CONFIG_STATE state = KEY;

	while ((bytes_read = fread(buf, sizeof(*buf), 4096, config_file)) > 0) {
		for (i = 0; i < bytes_read; ++i) {
			switch (state) {
			case COMMENT:
				if (buf[i] == '\n') {
					line_number++;
					state = KEY;
					key.str_length = 0;
					value.str_length = 0;
				}
				continue;
			case KEY:
				if (buf[i] == '\n') {
					key.str_length = 0;
					value.str_length = 0;
					line_number++;
				} else if (key.str_length == 0 &&
				           (isblank(buf[i]) ||
				            isspace(buf[i]))) {
					/* skip trailing white space */
				} else if (key.str_length == 0 &&
				           buf[i] == '#') {
					state = COMMENT;
				} else if (buf[i] == '=') {
					state = VALUE;
				} else {
					conf_str_append(&key, buf[i]);
				}
				break;
			case VALUE:
				if (buf[i] == '#') {
					state = COMMENT;
					conf_str_set_value(&key, &value,
					                   line_number);
				} else if (buf[i] == '\n') {
					state = KEY;
					conf_str_set_value(&key, &value,
					                   line_number);
					line_number++;
				} else {
					conf_str_append(&value, buf[i]);
				}
				break;
			}
		}
	}

	if (value.str_length > 0) /* if file does not end with new line */
		conf_str_set_value(&key, &value, line_number);

	free(key.str);
	free(value.str);

	fclose(config_file);
}

void
read_conf() {
	struct queue_method *method;
	int queue_method_count = 0, queue_weight_count = 0;
	FILE *config_file = NULL;
	const char *env_var;
	size_t i;

	if (conf.mpd_host != NULL)
		return;

	if ((env_var = getenv("MPD_PORT")) != NULL)
		conf.mpd_port = atoi(env_var);

	conf.cache_dir = get_cache_dir();

	config_file = get_config_file();

	if (config_file != NULL)
		parse_config(config_file);
	else
		log_data(LOG_ERROR, "Unable to read config");

	if (conf.mpd_host == NULL) {
		if ((env_var = getenv("MPD_HOST")) != NULL) {
			conf.mpd_host = malloc(strlen(env_var) + 1);
			strcpy(conf.mpd_host, env_var);
		} else {
			conf.mpd_host = malloc(strlen("localhost") + 1);
			strcpy(conf.mpd_host, "localhost");
		}
	}

	if (conf.queue_methods.length == 0) {
		method = malloc(sizeof(*method));
		*method = (struct queue_method){QM_SAME_ARTIST, 3};
		append_to_list(&conf.queue_methods, method);

		if (conf.lastfm_api_key != NULL) {
			method = malloc(sizeof(*method));
			*method = (struct queue_method){QM_RELATED_ARTIST, 4};
			append_to_list(&conf.queue_methods, method);
		} else {
			log_data(LOG_WARNING, "Last.fm api key not provided in "
			                      "configuration file");
		}

		method = malloc(sizeof(*method));
		*method = (struct queue_method){QM_RANDOM, 1};
		append_to_list(&conf.queue_methods, method);

	} else {
		for (i = 0; i < conf.queue_methods.length; ++i) {
			method = conf.queue_methods.items[i];
			if (method->type != QM_UNDEFINED)
				queue_method_count++;
			if (method->weight > 0)
				queue_weight_count++;
			if (conf.lastfm_api_key == NULL &&
			    method->type == QM_RELATED_ARTIST) {
				log_data(LOG_ERROR, "Cannot use queue method "
				                    "\"related_artist\" "
				                    "without providing last.fm "
				                    " api key");
				exit(1);
			}
		}

		if (queue_method_count != queue_weight_count) {
			log_data(LOG_ERROR,
			         "Amount of queue methods does not match the "
			         "amount of provied queue weights, %d methods "
			         "provied "
			         "while %d weights were provieded",
			         queue_method_count, queue_weight_count);
			exit(1);
		}
	}
}

const struct config *
get_conf_data() {
	return &conf;
}

void
free_config() {
	free(conf.mpd_host);
	free(conf.lastfm_api_key);
	free(conf.cache_dir);
	free(conf.mpd_password);
	free_list(&conf.queue_methods, &free);
}
