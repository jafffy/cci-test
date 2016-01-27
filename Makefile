LIB=-lcci -lpthread

all: server client
	echo "Done"
server: server.c
	gcc -o server server.c $(LIB)

client: client.c
	gcc -o client client.c $(LIB) 
