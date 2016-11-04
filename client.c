#include "chat.h"

/* ---------------------------------------------------------------------
 This is a sample client program for the number server. The client and
 the server need not run on the same machine.
 --------------------------------------------------------------------- */
 
int main(int argc, char *argv[])
{
    if( argc < 4 ) 
    {
      printf("Too few arguments supplied.\n");
      exit(0);
    }
    else if( argc > 4 ) 
    {
      printf("Too many arguments supplied.\n");
      exit(0);
    }

    if(strlen(argv[3]) >= MAX_NAME)
    {
        printf("Username too long!\n");
        exit(0);
    }
    
    int	s, i, j;
    unsigned char handbuf[2] = {0};
    unsigned char namebuf[MAX_NAME];
    char msgbuf[MAX_MSG];
    unsigned short clients;
    unsigned short msglength;
    char length;
    pid_t chid;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN, &readfds);

    struct	sockaddr_in	server;

    s = socket (AF_INET, SOCK_STREAM, 0);

    if (s < 0)
    {
        perror ("Client: cannot open socket");
        exit (1);
    }

    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));

    if (connect (s, (struct sockaddr*) & server, sizeof (server)))
    {
        perror ("Client: cannot connect to server");
        exit (1);
    }

    //Get handshake bytes
    if(my_recv(s, handbuf, sizeof(handbuf), 0) != sizeof(handbuf))
    {
        perror ("Client: cannot receive handshake");
    }

    if(my_recv(s, &clients, sizeof(clients), 0) != sizeof(clients))
    {
        perror ("Client: cannot receive number of clients");
    }

    clients = ntohs(clients);
    printf("\n%d user(s) online\n", clients);

    if(clients > 0)
    {
    	printf("--------------------------------------------------------\n");
    }

    //Receive list of usernames
    for(i=0; i < clients; ++i)
    {
        if(my_recv(s, &length, sizeof(length), 0) < 0)
        {
            perror ("Client: cannot receive length");
        }

        char name[(int)length];

        if(my_recv(s, &name, (int)length, 0) < 0)
        {
            perror ("Client: cannot receive Username");
        }

        for(j = 0 ; j < (int)length; ++j)
        {
            printf("%c",name[j]);
        }
        printf("\n");
    }

    if(clients > 0)
    {
    	printf("--------------------------------------------------------\n");
    }

    //Send new username
    length = (char)strlen(argv[3]);
    my_send(s, &length, sizeof(length), 0);
    my_send(s, &argv[3][0], (int)length, 0);

    printf("\n");

    chid = fork(); //Fork to have two loops, one for sending messages, one for receiving.

    if(chid == -1)	//Fork Error
    {	
    	//Let process terminate to end
    	perror("Fork Error");
    }
    if (chid > 0) //Executed by parent
    {
    	fd_set readfds;
    	FD_ZERO(&readfds);
    	FD_SET(STDIN, &readfds);

    	struct timeval tv;

        while(1)
        {
        	tv.tv_sec = TIMEOUT - 5;			//TIMEOUT-5 to be safe
    		tv.tv_usec = 0;
        	if(select(STDIN + 1, &readfds, NULL, NULL, &tv) == -1)
        	{
            	perror("select");
            	exit(-1);
        	}

        	if (FD_ISSET(STDIN, &readfds)){
        		//Something to read
	            i = 0;
	            while((msgbuf[i] = getchar()) != '\n' && msgbuf[i] != EOF)
	            {
	                ++i;
	                if(i == MAX_MSG - 1) break;
	            }

	            //Send Message Length
	            msglength = (short)i;
	            msglength = htons(msglength);
	            my_send(s, &msglength, sizeof(msglength), 0);

	            //Send Message Contents
	            msglength = ntohs(msglength);
	            my_send(s, msgbuf, msglength, 0);
	        }
	        else
	        {
	        	//Send Timeout Message
	        	msglength = 0;
	            msglength = htons(msglength);
	            if(my_send(s, &msglength, sizeof(msglength), 0) != sizeof(msglength)){
	            	printf("Timeout Message Not Sent!");
	            }

	        }
        }
    }

    if (chid == 0)	//Executed by child
    {
    	char msg_type;
    	uint16_t msg_userlength;
    	unsigned short convert; // convert as storage for ntos()
    	char msg_username[MAX_NAME];
    	uint16_t msg_msglength;
    	char msg_msg[100];

        while(1)
        {
            //Get message type
            if(my_recv(s, &msg_type, 1, 0) <= 0){
            	//Close all processes
                printf("Diconnected from server.\n");
            	close(s);
            	kill(getppid(), SIGTERM);
            	exit(0);
            }

            if (msg_type == CHAT_MSG)
            {
                //Get username
                my_recv(s, &length, sizeof(length), 0);
                my_recv(s, msg_username, (int) length, 0);
                msg_username[(int) length] = '\0';

                //Get message
                my_recv(s, &convert, sizeof(convert), 0);
                convert = ntohs(convert);
                my_recv(s, msgbuf, convert, 0);
                msgbuf[convert] = '\0';
                printf("<%s> %s\n", msg_username, msgbuf);
            }

            else if (msg_type == JOIN_MSG)
            {
                //Get username
                my_recv(s, &length, sizeof(length), 0);
                my_recv(s, msg_username, (int) length, 0);
                msg_username[(int) length] = '\0';

                printf("** %s has connected to the server **\n",msg_username);
            }

            else if (msg_type == LEAVE_MSG)
            {
                //Get username
                my_recv(s, &length, sizeof(length), 0);
                my_recv(s, msg_username, (int) length, 0);
                msg_username[(int) length] = '\0';

                printf("** %s has disconnected to the server **\n",msg_username);
            }
        }

    }

}

