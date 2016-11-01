#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include<signal.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>

#define MY_PORT 2222   // port we're listening on
#define MAX_NAME 30   // maximum length of a username
#define MAX_MSG 256	//Max length of chat message
#define STDIN 0  // file descriptor for standard input


