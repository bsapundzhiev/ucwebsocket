CC=gcc
CFLAGS=-c -g -Wall -I ../lib
LDFLAGS=
SOURCES=server.c websocket.c wshandshake.c base64.c sha1.c


OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=wsocket
OUTPUTFILE=libwsocket.a

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

static: all
	ar ru $(OUTPUTFILE) $(OBJECTS)
	ranlib $(OUTPUTFILE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) $(OUTPUTFILE)

format:
	astyle --style=stroustrup -s4 $(SOURCES)
