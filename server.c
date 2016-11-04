/*
** selectserver.c -- a cheezy multiperson chat server
** Link: http://beej.us/guide/bgnet/output/html/multipage/advanced.html#select
** Modified by: mwaqar
** 		Blaz Pocrnja
*/

#include "chat.h"

int file_number = 0;
FILE *flog = NULL;

void sigtstp_handler(int signo) {
    fprintf(flog,"SIGTSTP received.\n");
    //printf("SIGTSTP received.\n");
	fclose(flog);
	return;
}

void sigint_handler(int signo){
    fprintf(flog,"SIGINT received.\n");
    //printf("SIGTINT received.\n");
	fclose(flog);
	exit(0);
}

void sigterm_handler(int signum)
{
    fprintf(flog,"SIGTERM received.\n");
    //printf("SIGTERM received.\n");
	fclose(flog);
	exit(0);
}


int main(int argc, char *argv[])
{
    if( argc < 2 ) 
    {
      printf("Too few arguments supplied.\n");
      exit(0);
    }
    else if( argc > 2 ) 
    {
      printf("Too many arguments supplied.\n");
      exit(0);
    }

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
    char outbyte;
    char length;
    unsigned short msglength;
    char tmpname[MAX_NAME];
    typedef enum { false, true } bool;

    struct timeval tv;      

    time_t activitytime[FD_SETSIZE] = {0}; 
    time_t current = 0;

    pid_t pid = 0;
    pid_t sid = 0;

    //Log File Data
    char filename[32];

    //Setting up signal handler for SIGTSTP
	struct sigaction sigtstp_act;
	//reset flags -- fixes seg fault
	sigtstp_act.sa_flags = 0;
	sigtstp_act.sa_handler = sigtstp_handler;
	sigemptyset(&sigtstp_act.sa_mask);
	sigaction(SIGTSTP, &sigtstp_act, NULL);

	//Setting up signal handler for SIGINT
	struct sigaction sigint_act;
	//reset flags -- fixes seg fault
	sigint_act.sa_flags = 0;
	sigint_act.sa_handler = sigint_handler;
	sigemptyset(&sigint_act.sa_mask);
    sigaction(SIGINT, &sigint_act, NULL);

    //Setting up signal handler for SIGTERM
	struct sigaction sigterm_act;
	//reset flags -- fixes seg fault
	sigterm_act.sa_flags = 0;
	sigterm_act.sa_handler = sigterm_handler;
	sigemptyset(&sigterm_act.sa_mask);
    sigaction(SIGTERM, &sigterm_act, NULL);

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

    if(usernames == (name *)(-1))
    {
        fprintf(flog, "shmat [%s]\n", strerror(errno));
    }

    //Create Daemon
    pid = fork();

    if (pid < 0)
    {
        printf("fork failed!\n");
        exit(1);
    }

    if (pid > 0)
    {
    	// in the parent
       printf("pid of child process %d \n", pid);
       exit(0); 
    }

    //Print process ID for reference
    printf("Daemon PID: %d\n", getpid());

    umask(0);

	// open a log file
    snprintf(filename, sizeof(char) * 32, "server379%d.log", getpid());

    flog = fopen(filename, "w");
    if (flog == NULL)
	{
	    printf("Error opening log file!\n");
	    exit(1);
    }
    
    // create new process group -- don't want to look like an orphan
    sid = setsid();
    if(sid < 0)
    {
    	fprintf(flog, "cannot create new process group");
        exit(1);
    }
    
    /* Change the current working directory */ 
    if ((chdir("/")) < 0) {
      printf("Could not change working directory to /\n");
      exit(1);
    }		

    listener = socket(AF_INET, SOCK_STREAM, 0);

    // get us a socket and bind it
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = htons(atoi(argv[1]));

    if (bind(listener, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
        fprintf(flog, "bind [%s]\n", strerror(errno));
        exit(-1);
    }

    // listen
    if (listen(listener, 10) == -1)
    {
        fprintf(flog, "listen [%s]\n", strerror(errno));
        exit(-1);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener; // so far, it's this one

    // main loop
    for(;;)
    {
        read_fds = master; // copy it

        //Select makes tv undefined. So reset it
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1)
        {
            fprintf(flog, "select [%s]\n", strerror(errno));
            exit(-1);
        }

        //Update Current Time
        current = time(NULL);

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++)
        {

            //CHECK IF TIMED OUT
            if(i > listener)
            {
                if((*usernames)[i-listener-1][0] != 0 && (difftime(current,activitytime[i-listener-1]) >= TIMEOUT))
                {
                    close(i);                   // bye!
                    FD_CLR(i, &master);         //remove from master set
                    FD_CLR(i, &read_fds);       //remove from read set
                    --clients;                  //Decrement number of clients connected
                    (*usernames)[i-listener-1][0] = 0; //Nullify username

                    //Save length
                    length = (*usernames)[i-listener-1][0];

                    //Put username in buffer
                    for(k = 1; k <= (int)length; ++k)
                    {
                        buf[k-1] = (*usernames)[i-listener - 1][k];
                    }

                    outbyte = LEAVE_MSG;

                    //Forward disconnection to all clients
                    for(j = listener + 1; j <= fdmax; j++)
                    {
                        // send if name has been set
                        if (FD_ISSET(j, &master) && (*usernames)[j-listener-1][0] != 0)
                        {

                            //Send User Update Message: Leave
                            if(my_send(j, &outbyte, 1, 0) != 1)
                            {
                                fprintf(flog, "Could not send disconnection message to socket %d [%s]\n", j, strerror(errno));
                            }

                            if(my_send(j, &length, 1, 0) != 1)
                            {
                                fprintf(flog, "Could not send disconnection length to socket %d [%s]\n", j, strerror(errno));
                            }

                            if(my_send(j, buf, (int)length, 0) != (int) length)
                            {
                                fprintf(flog, "Could not send disconnection username to socket %d [%s]\n", j, strerror(errno));
                            }
                        }
                    }


                    fprintf(flog, "Socket %d Timed Out [%s]\n",i, strerror(errno));
                    continue;
                }
            }

            if (FD_ISSET(i, &read_fds))   // we got one!!
            {
                if (i == listener)
                {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);

                    if (newfd == -1)
                    {
                        fprintf(flog, "accept [%s]\n", strerror(errno));
                    }
                    else
                    {

                        FD_SET(newfd, &master); // add to master set

                        if (newfd > fdmax)      // keep track of the max
                        {
                            fdmax = newfd;
                        }

                        fprintf(flog, "new connection from %s:%d on socket %d [%s]\n",
                               inet_ntoa(remoteaddr.sin_addr), ntohs(remoteaddr.sin_port), newfd, strerror(errno));

                        //---------HANDSHAKE PROTOCOL------------

                        //First Two Bytes
                        buf[0] = HAND_1;
                        buf[1] = HAND_2;

                        if(my_send(newfd, buf, 2, 0) != 2)
                        {
                            fprintf(flog, "Handshake Not Sent [%s]\n", strerror(errno));
                        }

                        //Number of Clients
                        outnum = htons(clients);		//Change byte order before sending to client

                        if(my_send(newfd, &outnum, sizeof(outnum), 0) != 2)
                        {
                            fprintf(flog, "Client number not sent [%s]\n", strerror(errno));
                        }

                        //Usernames
                        int clienttmp = clients;

                        for(k = 0; k < clienttmp; ++k)
                        {
                            //if user is still connected
                            if((*usernames)[k][0] != 0)
                            {
                                //Send length
                                outbyte = (*usernames)[k][0];

                                if(my_send(newfd, &outbyte, sizeof(outbyte), 0) != sizeof(outbyte))
                                {
                                    fprintf(flog, "Username length not sent [%s]\n", strerror(errno));
                                }

                                //Send array of chars
                                for(l = 1; l <= (int)outbyte; ++l)
                                {
                                    buf[l - 1] = (*usernames)[k][l];
                                }

                                if(my_send(newfd, buf, (int)outbyte, 0) != (int) outbyte)
                                {
                                    fprintf(flog, "Username not sent [%s]\n", strerror(errno));
                                }
                            }
                            else
                            {
                                clienttmp++;
                            }

                        }

                        (*usernames)[newfd-listener-1][0] = 0; //Nullify username

                    }
                }
                else
                {
                    //handle data from a client

                    //Check if connection closed
                    if ((nbytes = my_recv(i, buf, 1, 0)) <= 0)
                    {
                        // got error or connection closed by client
                        if (nbytes == 0)
                        {
                            // connection closed
                            fprintf(flog, "socket %d hung up [%s]\n", i, strerror(errno));
                            
                        }
                        else
                        {
                            fprintf(flog, "Could not receive initial byte [%s]\n", strerror(errno));
                        }

                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set

                        if((*usernames)[i-listener - 1][0] != 0)
                        {
                            --clients;		//Decrement number of clients connected
                        }


                        //Save length
                        length = (*usernames)[i-listener-1][0];

                        //Nullify UserName
                        (*usernames)[i-listener-1][0] = 0;

                        //Put username in buffer
                        for(k = 1; k <= (int)length; ++k)
                        {
                            buf[k-1] = (*usernames)[i-listener - 1][k];
                        }

                        outbyte = LEAVE_MSG;

                        //Forward disconnection to all clients
                        for(j = listener + 1; j <= fdmax; j++)
                        {
                            // send if name has been set
                            if (FD_ISSET(j, &master) && (*usernames)[j-listener-1][0] != 0)
                            {

                                //Send User Update Message: Leave
                                if(my_send(j, &outbyte, 1, 0) != 1)
                                {
                                    fprintf(flog, "Could not send disconnection message to socket %d [%s]\n", j, strerror(errno));
                                }

                                if(my_send(j, &length, 1, 0) != 1)
                                {
                                    fprintf(flog, "Could not send disconnection length to socket %d [%s]\n", j, strerror(errno));
                                }

                                if(my_send(j, buf, (int)length, 0) != (int) length)
                                {
                                    fprintf(flog, "Could not send disconnection username to socket %d [%s]\n", j, strerror(errno));
                                }
                            }
                        }

                    }
                    else
                    {
                        // we got some data from a client
                        if((*usernames)[i-listener-1][0] == 0) 			//Username Message
                        {

                            //---------Continue Handshake Protocol---------------
                            length = buf[0];

                            //Receive New Username Chars
                            if(my_recv(i, buf, (int)length, 0) != (int) length)
                            {
                                fprintf(flog, "Could not receive username [%s]\n", strerror(errno));
                            }
			    
                            //Check if Username already exists
            			    bool exists = false;
            			    for(j = 0; j <= fdmax - listener - 1; ++j)
            			    {
            			    	if(length == (*usernames)[j][0])
            				    {
                					for(k = 1; k <= (int) length; ++k)
                					{
                						if(buf[k-1] != (*usernames)[j][k])
                						{
                							exists = false;
                							break;
                						}
                						else
                						{
                							exists = true;
                						}
                								
                					}
            				    }

            				    if(exists == true) break;
            			    }
				
            			    if(!exists)
            			    {
    		                    //Store in usernames array
    		                    (*usernames)[i-listener - 1][0] = length;

    		                    for(k = 1; k <= (int)length; ++k)
    		                    {
    		                        (*usernames)[i-listener - 1][k] = buf[k-1];
    		                        fprintf(flog,"%c",buf[k-1]);
    		                    }
    		                    fprintf(flog," stored in array %d\n", i-listener - 1);
    		                    ++clients;				//Increment number of clients connected

    		                    outbyte = JOIN_MSG;

    		                    //Forward new connection to all clients
    		                    for(j = listener + 1; j <= fdmax; j++)
    		                    {
    		                        // send if name has been set
    		                        if (FD_ISSET(j, &master) && (*usernames)[j-listener-1][0] != 0)
    		                        {

    		                            //Send User Update Message: Join
    		                            if(my_send(j, &outbyte, 1, 0) != 1)
    		                            {
    		                                fprintf(flog, "Could not send connection message to socket %d [%s]\n", j, strerror(errno));
    		                            }

    		                            if(my_send(j, &length, 1, 0) != 1)
    		                            {
    		                                fprintf(flog, "Could not send connection length to socket %d [%s]\n", j, strerror(errno));
    		                            }

    		                            if(my_send(j, buf, (int)length, 0) != (int) length)
    		                            {
    		                                fprintf(flog, "Could not send connection username to socket %d [%s]\n", j, strerror(errno));
    		                            }
    		                        }
    		                    }
                            }
                            else
                            {
    				            close(i); // bye!
                            	FD_CLR(i, &master); // remove from master set
                                fprintf(flog, "Username not unique [%s]\n", strerror(errno));
                            }


                        }
                        else 							//Chat Message
                        {
                            //Receive Message Length
                            if(my_recv(i, buf+1, 1, 0) != 1)
                            {
                                fprintf(flog, "Could not receive messagelength [%s]\n", strerror(errno));
                            }

                            msglength = (buf[1] << 8) | buf[0];
                            msglength = ntohs(msglength);

                            if(msglength > 0){
                                //Receive Message
                                if(my_recv(i, buf, msglength, 0) != msglength)
                                {
                                    fprintf(flog, "Could not receive chat message [%s]\n", strerror(errno));
                                }

                                //Forward to all clients
                                for(j = listener + 1; j <= fdmax; j++)
                                {
                                    // send if name has been set
                                    if (FD_ISSET(j, &master) && (*usernames)[j-listener-1][0] != 0)
                                    {
                                        //Send Message Code
                                        outbyte = CHAT_MSG;

                                        if (my_send(j, &outbyte, sizeof(outbyte), 0) != sizeof(outbyte))
                                        {
                                            fprintf(flog, "Chat message code not sent to FD %d [%s]\n", j, strerror(errno));
                                        }

                                        //Send Client info
                                        length = (*usernames)[i-listener-1][0];

                                        for(k = 0; k <= ((int)length); ++k)
                                        {
                                            tmpname[k] = (*usernames)[i-listener-1][k];
                                        }

                                        if(my_send(j, tmpname, ((int)length) + 1, 0) != ((int)length) + 1)
                                        {
                                            fprintf(flog, "Client info not sent to FD %d [%s]\n", j, strerror(errno));
                                        }

                                        //Send Message Length
                                        unsigned short tmp = htons(msglength);

                                        if (my_send(j, &tmp, sizeof(tmp), 0) != sizeof(tmp))
                                        {
                                           fprintf(flog, "Chat message length not sent to FD %d [%s]\n", j, strerror(errno));
                                        }

                                        //Send Message
                                        if (my_send(j, buf, msglength, 0) != msglength)
                                        {
                                            fprintf(flog, "Chat message not sent to FD %d [%s]\n", j, strerror(errno));
                                        }
                                    }
                                }
                            }
                        }
                    }

                    //Record Activity Time
                    if(i > listener)
                    {
                        activitytime[i-listener-1] = time(NULL);
                    }

                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!

    return 0;
}
