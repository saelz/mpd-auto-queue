#ifndef MPD_AUTO_QUEUE_LIST_H
#define MPD_AUTO_QUEUE_LIST_H

#include <stddef.h>

/* clang-format off */
#define NEW_LIST {0, NULL}
/* clang-format on */

typedef void(free_func)(void *);

typedef struct list {
	size_t length;
	void **items;
} List;

List new_list();

void append_to_list(List *list, void *data);

void free_list(List *list, free_func);

#endif
