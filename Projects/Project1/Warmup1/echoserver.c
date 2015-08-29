#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#if 0
/* 
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

#define BUFSIZE 16

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoserver [options]\n"                                                    \
"options:\n"                                                                  \
"  -p                  Port (Default: 8888)\n"                                \
"  -n                  Maximum pending connections\n"                         \
"  -h                  Show this help message\n"                              

/* Helper Methods ==================================================== */

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* Main ============================================================= */

int main(int argc, char **argv) 
{
	int optionChar;
	char *portno = "8888";
	int maxnpending = 5;

	// Parse and set command line arguments
	while ((optionChar = getopt(argc, argv, "p:n:h")) != -1)
	{
		switch (optionChar)
		{
			case 'p': // listen-port
				portno = optarg;
				break;                                        
			case 'n': // server
				maxnpending = atoi(optarg);
				break; 
			case 'h': // help
				fprintf(stdout, "%s", USAGE);
				exit(0);
				break;           
			default:
				fprintf(stderr, "%s", USAGE);
				exit(1);
		}
	}


    int listeningSocket, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP


	// Setting getaddrinfo 'node' to NULL and passing AI_PASSIVE makes
	// addresses suitable for binding
    if ((rv = getaddrinfo(NULL, portno, &hints, &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo did not find address: %s\n", gai_strerror(rv));
        return 1;
    }

    // Loop through all addresses and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((listeningSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
		{
            perror("server: socket");
            continue;
        }

		// Set socket options:
		// SOL_SOCKET, SO_REUSEADDR	- Allows socket to use the same address
		// 		and port being used by another socket of the same protocal
		// 		type in the system (restarts on closed/killed process)
        if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) 
        {
            perror("setsockopt");
            exit(1);
        }

		// Bind socket to port
        if (bind(listeningSocket, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(listeningSocket);
            perror("server: bind");
            continue;
        }

        break;
    }

	// Free IP addresses
    freeaddrinfo(servinfo);

    if (p == NULL) 
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

	// Listen to socket connections
    if (listen(listeningSocket, maxnpending) == -1)
    {
        perror("listen");
        exit(1);
    }

	// Reap zombie processes
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1)
    {
        sin_size = sizeof their_addr;
        new_fd = accept(listeningSocket, (struct sockaddr *)&their_addr, &sin_size);

        if (new_fd == -1) 
        {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family, 
			get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
            
        printf("server: got connection from %s\n", s);

        if (!fork())
        {
			// this is the child process
			
			char buf[BUFSIZE];
			int numbytes = recv(new_fd, buf, BUFSIZE - 1, 0);
			if(numbytes < 0)
			{
				perror("Unable to retrieve inbound message");
				exit(1);
			}
			
			printf("Received message: %s\n", buf);
			
            close(listeningSocket); // child doesn't need the listener
            
            printf("Sending reply: %s\n", buf);
            
            if (send(new_fd, buf, 13, 0) == -1)
            {
                perror("send");
            }
            
            close(new_fd);
            exit(0);
        }
        
        close(new_fd);  // parent doesn't need this
    }

    return 0;
}





