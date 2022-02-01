#ifndef MPD_AUTO_QUEUE_SONG_MANAGER_H
#define MPD_AUTO_QUEUE_SONG_MANAGER_H

struct mpd_connection;

enum QUEUE_METHOD_TYPE{
	QM_SAME_ARTIST,
	QM_RELATED_ARTIST,
	QM_RANDOM,
	QM_UNDEFINED
};

struct queue_method{
	enum QUEUE_METHOD_TYPE type;
	int weight;
};

void
autoqueue(struct mpd_connection *conn);
#endif
