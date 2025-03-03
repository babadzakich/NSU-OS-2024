all: server client proxy

proxy: proxy.cpp
	g++ -lnsl -lsocket -lresolv -Wall proxy.cpp -o proxy

client: client.c
	gcc -lnsl -lsocket -lresolv -Wall client.c -o client

server: server.c
	gcc -lnsl -lsocket -lresolv -Wall server.c -o server