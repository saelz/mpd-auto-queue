#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mpd/client.h>
#include <sys/select.h>

#include "conf.h"
#include "log.h"
#include "song_manager.h"

static void
msleep(unsigned long milliseconds){
	struct timeval len;
	len.tv_sec = milliseconds/1000;
	len.tv_usec = (milliseconds%1000) * 1000;
	select(0,NULL,NULL,NULL,&len);
}

int
main(int argc, char *argv[]){
	const struct config *conf;
	struct mpd_connection *conn;
	struct mpd_status *status;
	int queue_len = 0;
	int song_pos = 0;
	int i;

	for (i = 0; i < argc; ++i)
		if (!strcmp(argv[i],"--verbose"))
			toggle_verbose_logging(1);


	read_conf();
	conf = get_conf_data();

	srand(time(NULL));

	if ((conn = mpd_connection_new(conf->mpd_host,conf->mpd_port,0)) == NULL){
		log_data(LOG_ERROR,"Out of memory");
		return 1;
	}


	if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS){
		log_data(LOG_ERROR,"Unable to connect to mpd server: %s",
				mpd_connection_get_error_message(conn));
		mpd_connection_free(conn);
		free_config();
		return 1;
	}

	log_data(LOG_INFO,"Connected to MPD server: %s:%d",conf->mpd_host,
			 conf->mpd_port);


	for (;;) {
		mpd_send_status(conn);
		status = mpd_recv_status(conn);
		mpd_response_finish(conn);

		queue_len = mpd_status_get_queue_length(status);
		song_pos = mpd_status_get_song_pos(status);

		if (queue_len > 0 && queue_len -song_pos <= conf->min_songs_left)
			autoqueue(conn);

		mpd_status_free(status);
		mpd_run_idle_mask(conn, MPD_IDLE_QUEUE|MPD_IDLE_PLAYER);

		/* sleep for a bit in case multiple songs are being added to the
		   playlist at the same time*/
		msleep(250);
	}


	free_config();
	mpd_connection_free(conn);
	return 0;
}
