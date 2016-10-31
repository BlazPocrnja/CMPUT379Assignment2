/*
** selectserver.c -- a cheezy multiperson chat server
** Link: http://beej.us/guide/bgnet/output/html/multipage/advanced.html#select
** Modified by: mwaqar
** 		Blaz Pocrnja
*/

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

#define MY_PORT 2222   // port we're listening on
#define MAX_NAME 30   // maximum length of a username
int main(void)
{
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_in sa; 
    struct sockaddr_in remoteaddr; // client address
    socklen_t addrlen;

    char buf[256];    // buffer for client data
    int nbytes;
    
    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, j, rv;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    unsigned char handbuf[2] = {0xCF, 0xA7};	//handshake buffer to send
    unsigned short clients = 0;
    unsigned short outnum;
    int pid;
    char outbyte;

    /* Declare shared memory variables */
    key_t key; 
    int shmid;
    typedef char name[FD_SETSIZE][MAX_NAME];
    name *usernames;				//Array of usernames for each possible file descriptor

     /* Initialize Shared Memory */
     key = ftok("server.c",'R');
     shmid = shmget(key, FD_SETSIZE*MAX_NAME, 0644 | IPC_CREAT);

     /* Attach to Shared Memory */
     usernames = shmat(shmid, (void *)0, 0);
     if(usernames == (name *)(-1)){
     	perror("shmat");
     }

	//Test with data
	/*
	(*usernames)[0][0] = (char)3;
	(*usernames)[0][1] = 'n';
	(*usernames)[0][2] = 'e';
	(*usernames)[0][3] = 'w';
	*/

    listener = socket(AF_INET, SOCK_STREAM, 0);
	
    // get us a socket and bind it
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons(MY_PORT);
           
    if (bind(listener, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        perror("bind");
    	exit(-1);
    }

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(-1);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(-1);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
			
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s:%d on socket %d\n",
                        inet_ntoa(remoteaddr.sin_addr), ntohs(remoteaddr.sin_port), newfd);

			++clients;				//Increment number of clients connected
			
			//Fork server to do Handshake
			if ((pid = fork()) == -1){
			//Fork Error
				perror("Server could not be forked!");
				continue;
        		}
			else if(pid > 0){
			//Master
				continue;
			}
			else if(pid == 0){
			//Child 

				/*---------Initial Handshake Protocol-------------*/
				//send handshake to new client
				//we don't have to reorder bytes since we're sending single byte array
			        if(send(newfd, handbuf, sizeof(handbuf), 0) == -1){
					perror("Handshake send failure");
				}
				outnum = htons(clients);		//Change byte order before sending to client
				send(newfd, &outnum, sizeof(outnum), 0);
				
				
				int k;
				int l;
				for(k = 0; k < clients; ++k){
					//Send length 
					outbyte = (*usernames)[k][0];
					send(newfd, &outbyte, sizeof(outbyte), 0);

					//Send array of chars
					for(l = 1; l <= (int)outbyte; ++l){
						buf[l - 1] = (*usernames)[k][l];
					}
					send(newfd, buf, (int)outbyte, 0);
				}

				//Recieve New Username length
				recv(newfd, buf, sizeof(char), 0);
				//Recieve New Username Chars
				recv(newfd, &buf[1], (int)buf[0], 0);

				for(k = 0; k <= (int)buf[0]; ++k){
					(*usernames)[newfd-listener - 1][k] = buf[k];
				}
				printf("Stored in  array %d\n" , newfd-listener - 1);
								
				exit(0);				//Exit Child Process
			}
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set

			--clients;		//Decrement number of clients connected
                    } else {
                        // we got some data from a client
                        for(j = 0; j <= fdmax; j++) {
                            // send to everyone!
                            if (FD_ISSET(j, &master)) {
                                // except the listener and ourselves
                                if (j != listener && j != i) {
                                    if (send(j, buf, nbytes, 0) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}
