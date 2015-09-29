#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <termios.h>

#include "common_p.h"
#include "server_p.h"

/* get sockaddr, IPv4 or IPv6 */
void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static int recv_all(int sockfd, char *buf, int len)
{
	int bytes_left = len;
	int nbytes;
	char *tmp = buf;

	while (bytes_left > 0) {
		nbytes = recv(sockfd, tmp, bytes_left, 0);
		if(nbytes <= 0) {
			return nbytes;
		}

		bytes_left -= nbytes;
		tmp += nbytes;
	}

	return len;	
}

int main(int argc, char **argv)
{
    int sockfd, numbytes;
    char buf[MAX_BUF_LEN];
    struct addrinfo hints, *serverinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if(argc != 2) {
        fprintf(stderr, "usage: %s hostname\n", argv[0]);
        exit(1);
    } 

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((rv = getaddrinfo(argv[1], TCP_PORT, &hints, &serverinfo)) != 0) {
        fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = serverinfo; p != NULL; p = p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        
        break;
    }
 
	if(p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr*)p->ai_addr), s, sizeof(s));

    freeaddrinfo(serverinfo);

    printf("client: connecting to %s \n", s);

	numbytes = recv_all(sockfd, buf, 3);
	if(numbytes < 3) {
		fprintf(stderr,"recv incomplete; expected 3 got %d \n", numbytes);
		goto out;
	}

    buf[numbytes] = '\0';

    printf("client: received %d bytes '%s' \n", numbytes, buf);

out:
    close(sockfd);
	
    return 0;
}
