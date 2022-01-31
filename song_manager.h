#ifndef MPD_AUTO_QUEUE_SONG_MANAGER_H
#define MPD_AUTO_QUEUE_SONG_MANAGER_H

struct mpd_connection;

void
autoqueue(struct mpd_connection *conn);
#endif
