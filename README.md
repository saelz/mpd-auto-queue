# mpd-auto-queue
mpd-auto-queue is a program that will automatically enqueue songs to an MPD server
by checking [Last.fm](https://last.fm) for similar artist recommendations based
on currently playing songs.

## Configuration
You can configure mpd-auto-queue via it's [configuration file](mpd-auto-queue.conf),
which should be located at `~/.config/mpd-auto-queue/mpd-auto-queue.conf`
(it will be automatically created when you run `make install`) if not you can
just create it.
### Configuration file variables
* `mpd_host` The host url of the MPD server. `default:localhost`

* `mpd_port` The port used by the MPD server. `default:6600`

* `mpd_password` The password used by the MPD server

* `lastfm_api_key` The api key used to request recommendations from.
[Last.fm](https://last.fm) you can obtain one
[here](https://www.last.fm/api/account/create). (It's only required if you use
the `related_artist` queue method.)

* `use_cache` Whether or not mpd-auto-queue should cache [Last.fm](https://last.fm) 
api requests. `default:t`

* `min_songs_left` Enqueue songs when number of songs in queue that have yet
to be played is less than the specified amount. `default:5`

* `auto_queue_amount` The number of songs to enqueue when queue has less than
`min_songs_left`. `default:10`

* `queue_methods` When deciding what songs to enqueue mpd-auto-queue will choose
from one of the specified methods to select the next song.
`default:[same_artist, related_artist, random]`

* `queue_method_weights` The weights used to decide what the next queue method
will be. (The amount of weights must match the amount of `queue_methods`.)
`default:[3, 4, 1]`

#### Queue methods
The methods mpd-auto-queue can use to queue songs (The current artist is decided by
the song at the end of the queue)

* `same_artist` Enqueues a random song by the current artist.

* `related_artist` Enqueues a random song by a related artist (Requires
`lastfm_api_key`.).

* `random` Enqueues a random song

## Installation
Dependencies:

* [libmpdclient](https://www.musicpd.org/libs/libmpdclient/)
* [libCurl](https://curl.se/libcurl/)

Compilation and installation:
```
make
make install
```
