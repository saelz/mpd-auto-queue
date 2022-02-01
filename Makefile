WARNING=-Wall -Werror -Wextra -Wdeclaration-after-statement

BUILD_NAME = mpd-auto-queue
LIBS=-lmpdclient -lcurl

CFLAGS=$(WARNING) -std=c99 -pedantic -g

LDFLAGS=$(LIBS)

SRC=main.c parser.c net.c song_manager.c list.c log.c conf.c
OBJS=$(SRC:.c=.o)

BIN_PATH ?=/bin

XDG_CONFIG_HOME ?=~/.config/
CONF_DIR=$(XDG_CONFIG_HOME)$(BUILD_NAME)
CONF_NAME=$(BUILD_NAME).conf

all: $(BUILD_NAME)

%.o: %.c *.h
	$(CC) -c -o $(@F) $(CFLAGS) $<

$(BUILD_NAME): $(OBJS)
	$(CC) -o $(BUILD_NAME) $(OBJS) $(LDFLAGS)

install:
	mkdir $(CONF_DIR) -p
	cp -n $(CONF_NAME) $(CONF_DIR)
	su -c "install -c $(BUILD_NAME) $(BIN_PATH)"


clean:
	rm $(OBJS)
	rm $(BUILD_NAME)
