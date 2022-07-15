.PHONY: all

all : server

server : server.cpp
	g++ server.cpp -o server -lsqlite3 -B C:\SQLite

