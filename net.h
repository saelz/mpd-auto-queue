#ifndef MPD_AUTO_QUEUE_NET
#define MPD_AUTO_QUEUE_NET
#include <stddef.h>

struct str {
	char *data;
	size_t size;
};

char *build_url(size_t count, /* int escape, char *url */...);

struct str request(const char *url);

#endif
