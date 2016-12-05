all: client.c server.c
	gcc -pthread -o client client.c
	gcc -pthread -o server server.c

clean:
	rm client server
