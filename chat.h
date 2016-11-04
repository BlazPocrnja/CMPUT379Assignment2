#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>

#define MY_PORT 2222   // port we're listening on
#define MAX_NAME 30   // maximum length of a username
#define MAX_MSG 256	//Max length of chat message
#define STDIN 0  // file descriptor for standard input

//Message Initializers
#define HAND_1 0xCF
#define HAND_2 0xA7
#define CHAT_MSG 0x00
#define JOIN_MSG 0x01
#define LEAVE_MSG 0x02

#define TIMEOUT 30


ssize_t my_send(int sockfd, const void *buf, size_t len, int flags){

	size_t total = 0;

	while ( total != len ) {
	    ssize_t nb = send( sockfd, buf + total, len - total, flags);
	    if ( nb == -1 ) return -1;
	    total += nb;
	}

	return total;
}


ssize_t my_recv(int sockfd, void *buf, size_t len, int flags){

	size_t total = 0; 

	while (total != len) {
	    ssize_t nb = recv( sockfd, buf + total, len - total, flags);
	    if ( nb == -1 ) return -1;
	    if (nb == 0) return 0;
	    total += nb;
	}

	return total;
}
