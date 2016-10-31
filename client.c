#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define	 MY_PORT  2222
#define MAX_NAME 30

/* ---------------------------------------------------------------------
 This is a sample client program for the number server. The client and
 the server need not run on the same machine.				 
 --------------------------------------------------------------------- */

int main()
{
	int	s, i , j;
	unsigned char handbuf[2] = {0};
	unsigned char namebuf[MAX_NAME];
	unsigned short clients;
	char length;

	struct	sockaddr_in	server;

	struct	hostent		*host;

	host = gethostbyname ("localhost");

	if (host == NULL) {
		perror ("Client: cannot get host description");
		exit (1);
	}
	
	s = socket (AF_INET, SOCK_STREAM, 0);

	if (s < 0) {
		perror ("Client: cannot open socket");
		exit (1);
	}

	bzero (&server, sizeof (server));
	bcopy (host->h_addr, & (server.sin_addr), host->h_length);
	server.sin_family = host->h_addrtype;
	server.sin_port = htons (MY_PORT);

	if (connect (s, (struct sockaddr*) & server, sizeof (server))) {
		perror ("Client: cannot connect to server");
		exit (1);
	}
	
	//we don't have to reorder bytes since we're recieving single byte array
	if(recv(s, handbuf, sizeof(handbuf), 0) < 0){
		perror ("Client: cannot receive handshake");
	}	
	
	printf("0x%x 0x%x\n", handbuf[0], handbuf[1]);
	
	if(recv(s, &clients, sizeof(clients), 0) < 0){
		perror ("Client: cannot receive number of clients");
	}
	clients = ntohs(clients);
	printf("Clients: %d\n", clients - 1);
	
	//Receive list of usernames
	for(i=0; i < clients - 1; ++i){
		if(recv(s, &length, sizeof(length), 0) < 0){
			perror ("Client: cannot receive length");
		}
		printf("Length: %d ", length);
		char name[(int)length];
		
		if(recv(s, &name, (int)length, 0) < 0){
			perror ("Client: cannot receive Username");
		}
		
		printf("Username: ");
		for(j = 0 ; j < (int)length; ++j){
			printf("%c",name[j]);
		}
		printf("\n");
		
	}

	//Send new username
	i= 0;
	printf("Enter a unique username: ");
	while((namebuf[i] = getchar()) != '\n' && namebuf[i] != EOF){
		++i;
		if(i == MAX_NAME - 1) break;
	}

	printf("Username: ");
	for(j = 0 ; j < i; ++j){
		printf("%c",namebuf[j]);
	}	
	printf("\n");
	
	length = (char)i;

	send(s, &length, sizeof(length), 0);
	send(s, &namebuf, (int) length, 0);
	
	while(1){
	}

	/*
	while (1) {

		s = socket (AF_INET, SOCK_STREAM, 0);

		if (s < 0) {
			perror ("Client: cannot open socket");
			exit (1);
		}

		bzero (&server, sizeof (server));
		bcopy (host->h_addr, & (server.sin_addr), host->h_length);
		server.sin_family = host->h_addrtype;
		server.sin_port = htons (MY_PORT);

		if (connect (s, (struct sockaddr*) & server, sizeof (server))) {
			perror ("Client: cannot connect to server");
			exit (1);
		}

		read (s, &number, sizeof (number));
		close (s);
		fprintf (stderr, "Process %d gets number %d\n", getpid (),
			ntohl (number));
		sleep (5);
	}
	*/
}
