#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpd/client.h>


#include <sys/stat.h>
#include <sys/select.h>

#include "song_manager.h"
#include "conf.h"
#include "log.h"

int verbose = 0;

char *cache_dir;

static void
msleep(unsigned long milliseconds){
	struct timeval len;
	len.tv_sec = milliseconds/1000;
	len.tv_usec = (milliseconds%1000) * 1000;
	select(0,NULL,NULL,NULL,&len);
}

int
main(int argc, char *argv[]){

	struct mpd_connection *conn;
	const char *cache_path,*home_path;
	struct mpd_status *status;
	int queue_len = 0;
	int song_pos = 0;
	int i;

	for (i = 0; i < argc; ++i) {
		if (!strcmp(argv[i],"--verbose"))
			verbose = 1;
	}


	/* TODO: read conf */

	srand(time(NULL));

	if ((conn = mpd_connection_new(MPD_HOST,MPD_PORT,0)) == NULL){
		log_data(LOG_ERROR,"Out of memory");
		return 1;
	}


	if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS){
		log_data(LOG_ERROR,"Unable to connect to mpd server: %s",
				mpd_connection_get_error_message(conn));
		mpd_connection_free(conn);
		return 1;
	}

	log_data(LOG_INFO,"Connected to MPD server: %s:%d",MPD_HOST,MPD_PORT);

	if ((cache_path = getenv("XDG_CACHE_HOME")) != NULL){
		cache_dir = malloc(strlen(cache_path)+strlen(PROGRAM_DIR_NAME)+1);
		strcpy(cache_dir, cache_path);
		strcat(cache_dir,PROGRAM_DIR_NAME);
	} else{
		if ((home_path = getenv("HOME")) != NULL)
			home_path = getpwuid(getuid())->pw_dir;
		cache_dir = malloc(strlen(home_path)+strlen("/.cache"PROGRAM_DIR_NAME)+1);
		strcpy(cache_dir, home_path);
		strcat(cache_dir,"/.cache"PROGRAM_DIR_NAME);
	}
	mkdir(cache_dir,0777);

	for (;;) {
		mpd_send_status(conn);
		status = mpd_recv_status(conn);
		mpd_response_finish(conn);

		queue_len = mpd_status_get_queue_length(status);
		song_pos = mpd_status_get_song_pos(status);

		if (queue_len > 0 && queue_len -song_pos <= min_songs_left)
			autoqueue(conn);

		mpd_status_free(status);
		mpd_run_idle_mask(conn, MPD_IDLE_QUEUE|MPD_IDLE_PLAYER);

		/* sleep for a bit in case multiple songs are being added to the
		   playlist at the same time*/
		msleep(250);
	}


	mpd_connection_free(conn);
	free(cache_dir);
	return 0;
}