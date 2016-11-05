all : client server

client : 
	gcc client.c -o chat379

server :
	gcc server.c -o server379

clean :
	rm -f chat379 server379
