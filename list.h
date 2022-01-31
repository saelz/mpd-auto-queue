#ifndef MPD_AUTO_QUEUE_LIST_H
#define MPD_AUTO_QUEUE_LIST_H

#include <stddef.h>

typedef void (free_func)(void*);

typedef struct list{
	size_t length;
	void **items;
} List;

List
new_list();

void
append_to_list(List *list,void *data);

void
free_list(List *list,free_func);

#endif
