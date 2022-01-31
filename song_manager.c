#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mpd/client.h>

#include "net.h"
#include "parser.h"
#include "song_manager.h"
#include "list.h"
#include "log.h"
#include "conf.h"

#define STR(x) {x,(size_t)strlen(x)}
#define STR_CMP(x,y) (strncmp(x.data,y.data,(y.size > x.size ? x.size : y.size)))

struct recommendations{
	struct str name;
	struct str mbid;
	double match;
};

static int
check_by_tag(struct mpd_connection *conn,struct str check,enum mpd_tag_type type){
	struct mpd_pair *pair;
	int found = 0;

	if (check.size <= 0)
		return 0;


	mpd_search_db_tags(conn, type);
	mpd_search_commit(conn);

	while ((pair = mpd_recv_pair_tag(conn, type)) != NULL) {
		if (strlen(pair->value) == check.size &&
			!strncmp(pair->value, check.data, check.size)){
			mpd_return_pair(conn, pair);
			found = 1;
			break;
		}

		mpd_return_pair(conn, pair);
	}

	mpd_response_finish(conn);
	return found;
}

static void
xml_create_rec_item(struct mpd_connection *conn,List *rec_list,
					 struct xml_item *item){
	struct recommendations *new_rec;
	struct xml_item *child;
	struct str name_tag = STR("name");
	struct str mbid_tag = STR("mbid");
	struct str sim_tag = STR("match");

	new_rec = malloc(sizeof(*new_rec));
	new_rec->match = 0;
	new_rec->name.size = 0;
	new_rec->mbid.size = 0;

	if ((child = find_xml_item_child(item,name_tag)) != NULL){
		new_rec->name = child->data;
	} else {
		free(new_rec);
		return;
	}

	if ((child = find_xml_item_child(item,mbid_tag)) != NULL)
		new_rec->mbid = child->data;

	if ((child = find_xml_item_child(item,sim_tag)) != NULL)
		new_rec->match = strtod(child->data.data,NULL);

	if (!check_by_tag(conn,new_rec->mbid,MPD_TAG_MUSICBRAINZ_ARTISTID) &&
		!check_by_tag(conn,new_rec->name,MPD_TAG_ARTIST)){
		free(new_rec);
		return;
	}

	log_data(LOG_VERBOSE, "Found related artist \"%.*s\" [%.*s]",
			 (int)new_rec->name.size,new_rec->name.data,
			 (int)new_rec->mbid.size,new_rec->mbid.data);

	append_to_list(rec_list, new_rec);
}

static List
xml_item_to_rec(struct mpd_connection *conn,struct xml_item *item){
	size_t i;
	struct xml_item *child;

	struct str lfm_tag = STR("lfm");
	struct str similar_tag = STR("similarartists");
	struct str artist_tag = STR("artist");

	List rec_list = new_list();

	if ((item = find_xml_item_child(item,lfm_tag)) == NULL){
		log_data(LOG_ERROR,"Unable to find lfm tag");
		return rec_list;
	}

	if ((item = find_xml_item_child(item,similar_tag)) == NULL){
		log_data(LOG_ERROR,"Unable to find similar artist tag");
		return rec_list;
	}

	for(i = 0; i < item->child_count; i++){
		child = item->children[i];

		if (!STR_CMP(child->name,artist_tag))
			xml_create_rec_item(conn,&rec_list, child);
	}


	return rec_list;
}

static struct mpd_song *
select_random_song_from_search(struct mpd_connection *conn,List playlist_entites){
	List song_list = new_list();
	const struct mpd_song *playlist_song;
	struct mpd_song *song;
	size_t i;
	int found,rng;

	while ((song = mpd_recv_song(conn)) != NULL){
		found = 0;
		for (i = 0; i < playlist_entites.length; ++i) {
			playlist_song = mpd_entity_get_song(playlist_entites.items[i]);
			if (!strcmp(mpd_song_get_uri(song),mpd_song_get_uri(playlist_song)))
				found = 1;
		}
		if (!found)
			append_to_list(&song_list, song);
		else
			mpd_song_free(song);
	}

	mpd_response_finish(conn);

	log_data(LOG_VERBOSE, "%d potential songs found",song_list.length);
	if (song_list.length < 1)
		return NULL;

	rng = rand()%song_list.length;
	song = song_list.items[rng];
	song_list.items[rng] = NULL;

	free_list(&song_list, (free_func*)mpd_song_free);

	return song;
}

static struct mpd_song *
get_related_song_by_artist(struct mpd_connection *conn,List playlist_entites,
						   struct str s_artist){
	char artist[s_artist.size+1];

	if (s_artist.size > 0){
		strncpy(artist, s_artist.data, s_artist.size);
		artist[s_artist.size] = '\0';
	}else return NULL;

	mpd_search_db_songs(conn,false);
	mpd_search_add_tag_constraint(conn, MPD_OPERATOR_DEFAULT,
								  MPD_TAG_ARTIST,artist);
	mpd_search_commit(conn);


	return select_random_song_from_search(conn,playlist_entites);
}


static struct mpd_song *
get_related_song_by_mbid(struct mpd_connection *conn,List playlist_entites,
						 struct str s_mbid){
	char mbid[s_mbid.size+1];

	if (s_mbid.size > 0){
		strncpy(mbid, s_mbid.data, s_mbid.size);
		mbid[s_mbid.size] = '\0';
	}else return NULL;

	mpd_search_db_songs(conn,false);
	mpd_search_add_tag_constraint(conn, MPD_OPERATOR_DEFAULT,
								  MPD_TAG_MUSICBRAINZ_ARTISTID,mbid);
	mpd_search_commit(conn);

	return select_random_song_from_search(conn,playlist_entites);
}

extern char *cache_dir;

static struct str
get_artist_data(List playlist_entites){
	struct str data = {NULL,0};
	char *url = NULL;
	FILE *cache_data = NULL;
	char *cache_file,*ptr;
	char buf[4096];
	size_t bytes_read;

	const char *artist =
		mpd_song_get_tag(mpd_entity_get_song(
							 playlist_entites.items[playlist_entites.length-1]),
						 MPD_TAG_ARTIST, 0);

	if (use_cache){
		cache_file = malloc(strlen(cache_dir)+strlen(artist)+strlen(".xml")+1);
		strcpy(cache_file,cache_dir);
		strcat(cache_file,artist);
		strcat(cache_file,".xml");
		cache_data = fopen(cache_file,"r");
	}

	if (cache_data != NULL){
		log_data(LOG_VERBOSE, "Reading related artist data from cache");
		while ((bytes_read = fread(buf, sizeof(*buf), 4096, cache_data)) > 0){
			ptr = realloc(data.data,data.size+bytes_read);
			if (ptr == NULL){
				free(cache_file);
				free(data.data);
				return (struct str){NULL,0};
			}
			data.data = ptr;

			memcpy(data.data+data.size, buf, bytes_read);
			data.size += bytes_read;
		}

		fclose(cache_data);
		free(cache_file);
	} else {
		url = build_url(4,
						0,"https://ws.audioscrobbler.com/2.0/?method=artist.getsimilar&artist=",
						1,artist,
						0,"&api_key=",
						0,lastfm_api_key);

		log_data(LOG_VERBOSE, "No related artist data found in cache requesting %s",
			url);

		data = request(url);

		if (use_cache){
			cache_data = fopen(cache_file,"w");
			if (cache_data != NULL){
				fwrite(data.data,sizeof(*data.data),data.size,cache_data);
				fclose(cache_data);
			}
			free(cache_file);
		}
		free(url);
	}


	return data;
}

static struct mpd_song *
get_related_song(struct mpd_connection *conn,List playlist_entites){
	struct str data;
	struct xml_item *item;
	struct recommendations *rec;
	List rec_list;
	struct mpd_song *related_song = NULL;
	int random;

	data = get_artist_data(playlist_entites);

	if (data.size > 0 && data.data != NULL){
		item = parse_xml(data);
		//print_xml_item(item,0);
		rec_list = xml_item_to_rec(conn,item);

		if (rec_list.length != 0){
			random = rand()%rec_list.length;
			if (random > 0)
				random = abs((rand()%(int)rec_list.length)-(rand()%random));

			rec = rec_list.items[random];

			log_data(LOG_VERBOSE, "Rolled %d",random);
			log_data(LOG_VERBOSE, "Using related artist: \"%.*s\" [%.*s]",
					 (int)rec->name.size,rec->name.data,
					 (int)rec->mbid.size,rec->mbid.data);

			related_song = get_related_song_by_mbid(conn,playlist_entites,
													rec->mbid);
			if (related_song == NULL)
				related_song = get_related_song_by_artist(conn,playlist_entites,
														  rec->name);
		}

		free_list(&rec_list,free);
		free(data.data);
		free_xml_item(item);
		return related_song;
	}

	log_data(LOG_WARNING,"Failed to retrieve last.fm recommendations");
	return NULL;
}

static struct mpd_song *
get_song_from_same_artist(struct mpd_connection *conn,List playlist_entites){
	struct mpd_song *random_song = NULL;
	const struct mpd_song *song = mpd_entity_get_song(
		playlist_entites.items[playlist_entites.length-1]);
	const char *mbid = mpd_song_get_tag(song, MPD_TAG_MUSICBRAINZ_ARTISTID, 0);
	const char *artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);

	if (mbid != NULL)
		random_song = get_related_song_by_mbid(conn,playlist_entites,
											   (struct str)STR((char*)mbid));
	else
		random_song = get_related_song_by_artist(conn,playlist_entites,
												 (struct str)STR((char*)artist));

	return random_song;
}

static struct mpd_song *
get_random_song(struct mpd_connection *conn,List playlist_entites){

	mpd_search_db_songs(conn,false);
	mpd_search_add_uri_constraint(conn, MPD_OPERATOR_DEFAULT, "");
	mpd_search_commit(conn);

	return select_random_song_from_search(conn,playlist_entites);
}

static void
queue_next_song(struct mpd_connection *conn,List playlist_entites){
	struct mpd_song *next_song = NULL;
	int rng = (rand()%100)+1;
	double weight_total = artist_weight+related_artist_weight+random_weight;
	double chance = 0;

	log_data(LOG_VERBOSE,"Rolled %d with ratio: %d:%d:%d = %lf %lf %lf",rng,
			 artist_weight,related_artist_weight,random_weight,
			 artist_weight/weight_total,related_artist_weight/weight_total,
			 random_weight/weight_total);


	if (artist_weight > 0 &&
		 rng <= (chance += artist_weight/weight_total)*100){
		log_data(LOG_INFO,"Queuing song by same artist");
		next_song = get_song_from_same_artist(conn,playlist_entites);
	} else if (related_artist_weight > 0 &&
			   rng <= (chance += related_artist_weight/weight_total)*100){
		log_data(LOG_INFO,"Queuing song by related artist");
		next_song = get_related_song(conn,playlist_entites);
	} else{
		log_data(LOG_INFO,"Queuing random song");
		next_song = get_random_song(conn,playlist_entites);
	}

	if (next_song == NULL){
		log_data(LOG_WARNING,"Unable to find song falling back to random");
		next_song = get_random_song(conn,playlist_entites);
		if (next_song == NULL){
			log_data(LOG_ERROR,"Still couldn't find a song");
			exit(1);
		}
	}

	log_data(LOG_INFO,"Queuing: %s", mpd_song_get_uri(next_song));
	mpd_send_add(conn, mpd_song_get_uri(next_song));
	mpd_response_finish(conn);
	mpd_song_free(next_song);
}


void
autoqueue(struct mpd_connection *conn){
	struct mpd_entity *entity;
	size_t i;
	List playlist_entites = new_list();

	for (i = 0; i < auto_queue_amount; ++i) {
		mpd_send_list_queue_meta(conn);

		while ((entity = mpd_recv_entity(conn)) != NULL){
			if (mpd_entity_get_type(entity) == MPD_ENTITY_TYPE_SONG)
				append_to_list(&playlist_entites, entity);
			else mpd_entity_free(entity);
		}

		mpd_response_finish(conn);

		queue_next_song(conn, playlist_entites);
		free_list(&playlist_entites,(free_func*)mpd_entity_free);
	}
}