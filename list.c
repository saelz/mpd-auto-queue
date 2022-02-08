#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "log.h"

List
new_list() {
	return (List)NEW_LIST;
}

void
append_to_list(List *list, void *data) {
	void *ptr;

	list->length++;
	ptr = realloc(list->items, (list->length * sizeof(*list->items)) + 1);
	if (ptr == NULL) {
		log_data(LOG_ERROR, "Error Out of memory");
		exit(1);
	}

	list->items = ptr;

	list->items[list->length - 1] = data;
}

void
free_list(List *list, free_func func) {
	size_t i;
	for (i = 0; i < list->length; ++i)
		if (list->items[i] != NULL)
			func(list->items[i]);

	free(list->items);
	list->length = 0;
	list->items = NULL;
}
