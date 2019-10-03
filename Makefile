# Makefile Tema 2 PC

CFLAGS = -Wall -g -std=c++11

all: server client

# Compileaza server.cpp
server: server.cpp
	g++ ${CFLAGS} server.cpp -o server

# Compileaza subscriber.cpp
subscriberTcp: subscriber.cpp
	g++ ${CFLAGS} subscriber.cpp -o subscriber


clean:
	rm -f server client
