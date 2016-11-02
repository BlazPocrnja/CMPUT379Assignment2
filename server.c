/*
** selectserver.c -- a cheezy multiperson chat server
** Link: http://beej.us/guide/bgnet/output/html/multipage/advanced.html#select
** Modified by: mwaqar
** 		Blaz Pocrnja
*/

#include "chat.h"

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
    int i, j, rv, k, l;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    unsigned short clients = 0;
    unsigned short outnum;
    int pid;
    char outbyte;
    char length;
    unsigned short msglength;

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

			//---------HANDSHAKE PROTOCOL------------

			//First Two Bytes
			buf[0] = HAND_1;
			buf[1] = HAND_2;
			if(my_send(newfd, buf, 2, 0) != 2){
				perror("Handshake Bytes not sent!");
			}
			
			//Number of Clients
			outnum = htons(clients);		//Change byte order before sending to client
			if(my_send(newfd, &outnum, sizeof(outnum), 0) != 2){
				perror("Number of clients not sent!");
			}
			
			//Usernames
			int clienttmp = clients;
			for(k = 0; k < clienttmp; ++k){
				//if user is still connected
				if((*usernames)[k][0] != 0){
					//Send length 
					outbyte = (*usernames)[k][0];
					if(my_send(newfd, &outbyte, sizeof(outbyte), 0) != sizeof(outbyte)){	
						perror("Length of username not sent!");		
					}

					//Send array of chars
					for(l = 1; l <= (int)outbyte; ++l){
						buf[l - 1] = (*usernames)[k][l];
					}
					if(my_send(newfd, buf, (int)outbyte, 0) != (int) outbyte){
						perror("Username not sent!");
					}
				}
				else{
					clienttmp++;
				}
				
			}
			
                    }
                } else {
	            //TODO FORK DATA HANDLING!!

                    // handle data from a client
			
		    //Check if connection closed		
                    if ((nbytes = my_recv(i, buf, 1, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("Could not receive initial byte!");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
			
			//Nullify UserName
			(*usernames)[i-listener-1][0] = 0; 
			--clients;		//Decrement number of clients connected

                    } else {
			printf("Data Handling");
                        // we got some data from a client
			if((*usernames)[i-listener-1][0] == 0){			//Username Message	

				//---------Continue Handshake Protocol---------------
				length = buf[0];

				//Receive New Username Chars
				if(my_recv(i, buf, (int)length, 0) != (int) length){
					perror("Could not receive new username!");
				}
				
				//TODO Check if Username already exists
				
				//Store in usernames array
				(*usernames)[i-listener - 1][0] = length;
				for(k = 1; k <= (int)length; ++k){
					(*usernames)[i-listener - 1][k] = buf[k-1];
					printf("%c",buf[k-1]);
				}
				printf("Stored in  array %d\n" , newfd-listener - 1);		
				++clients;				//Increment number of clients connected
			}
			else{							//Chat Message
				//TODO Handle Client messages
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
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    
    return 0;
}
