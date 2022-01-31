#ifndef MPD_AUTO_QUEUE_CONF
#define MPD_AUTO_QUEUE_CONF


#define MPD_HOST "192.168.1.102"
#define MPD_PORT 6666
#define PROGRAM_DIR_NAME "/mpd-auto-queue/"

static const char lastfm_api_key[] = ":^)";

static const int min_songs_left = 5;
static const unsigned auto_queue_amount = 10;

static const int use_cache = 1;

static const int artist_weight = 3;
static const int related_artist_weight = 4;
static const int random_weight = 1;

#endif
