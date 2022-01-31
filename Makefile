WARNING=-Wall -Werror -Wextra -Wdeclaration-after-statement

BUILD_NAME = mpd-auto-queue
LIBS=-lmpdclient -lcurl

CFLAGS=$(WARNING) -std=c99 -pedantic -g

LDFLAGS=$(LIBS)

SRC=main.c parser.c net.c song_manager.c list.c log.c
OBJS=$(SRC:.c=.o)

all: $(BUILD_NAME)

%.o: %.c *.h
	$(CC) -c -o $(@F) $(CFLAGS) $<

$(BUILD_NAME): $(OBJS)
	$(CC) -o $(BUILD_NAME) $(OBJS) $(LDFLAGS)

clean:
	rm $(OBJS)
	rm $(BUILD_NAME)
