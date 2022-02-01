#ifndef MPD_AUTO_QUEUE_CONF
#define MPD_AUTO_QUEUE_CONF

#include "list.h"


struct config{
	int mpd_port;
	char *mpd_host;
	char *lastfm_api_key;
	int use_cache;
	struct list queue_methods;
	char *cache_dir;
	int min_songs_left;
	int auto_queue_amount;
};

void
read_conf();

const struct config *
get_conf_data();

void
free_config();
#endif
