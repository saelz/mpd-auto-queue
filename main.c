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
	struct mpd_message *message;
	const char *message_text;
	int paused = 0;
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
	if (conf->mpd_password != NULL && !mpd_run_password(conn, conf->mpd_password)){
			log_data(LOG_WARNING, "%s",
					 mpd_connection_get_error_message(conn));
	}


	mpd_run_subscribe(conn, "mpd-auto-queue");
	for (;;) {

		mpd_send_read_messages(conn);
		while ((message = mpd_recv_message(conn)) != NULL){
			message_text = mpd_message_get_text(message);
			log_data(LOG_VERBOSE,"Received Message:%s",message_text);
			if (!strcmp(message_text,"exit")){
				mpd_response_finish(conn);
				mpd_message_free(message);
				goto end;
			} else if (!strcmp(message_text,"pause")){
				log_data(LOG_INFO,"Auto queuing has paused");
				paused = 1;
			} else if (!strcmp(message_text,"resume") || !strcmp(message_text,"unpause")){
				log_data(LOG_INFO,"Auto queuing has resumed");
				paused = 0;
			} else if (!strcmp(message_text,"toggle-pause")){
				paused = !paused;
				log_data(LOG_INFO,"Auto queuing has %s", paused ? "paused":"resumed");
			}
			mpd_message_free(message);
		}
		mpd_response_finish(conn);

		if (!paused){
			mpd_send_status(conn);
			status = mpd_recv_status(conn);
			mpd_response_finish(conn);
			if (status == NULL){
				log_data(LOG_ERROR, "Unable to retrive MPD status: %s",
						 mpd_connection_get_error_message(conn));
				break;
			}

			queue_len = mpd_status_get_queue_length(status);
			song_pos = mpd_status_get_song_pos(status);

			if (queue_len > 0 && queue_len -song_pos <= conf->min_songs_left)
				autoqueue(conn);
			mpd_status_free(status);
		}

		if (paused)
			mpd_run_idle_mask(conn,MPD_IDLE_MESSAGE);
		else
			mpd_run_idle_mask(conn, MPD_IDLE_QUEUE|MPD_IDLE_PLAYER|MPD_IDLE_MESSAGE);

		/* sleep for a bit in case multiple songs are being added to the
		   playlist at the same time*/
		msleep(250);
	}


end:
	log_data(LOG_INFO, "Exiting...");
	free_config();
	mpd_connection_free(conn);
	return 0;
}
