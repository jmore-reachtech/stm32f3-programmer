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
#include "serial.h"
#include "gpio.h"
#include "stm32.h"

static void sigchld_handler(int s)
{
	/* waitpid might overwrite errno, so we save and restore */
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

/* get sockaddr, IPv4 or IPv6 */
static void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static int handle_client(int socketfd)
{
    printf("server: sending 3 bytes '%s'\n", ISP_ACK);
    if(send(socketfd, ISP_ACK, 3 ,0) == -1) {
        perror("send");
    }

    return 0;
}

int main(int argc, char **argv)
{
    int sockfd, new_fd;
    struct addrinfo hints, *serverinfo, *p;
    struct sockaddr_storage client_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;
 
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, TCP_PORT, &hints, &serverinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    for(p = serverinfo; p != NULL; p = p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        
        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        
        break;
    }

    freeaddrinfo(serverinfo);
    
    if(p == NULL) {
        fprintf(stderr,"server: failed to bind\n");
        exit(1);
    }
 
    if(listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while(1) {
        sin_size = sizeof(client_addr);
	    printf("Server: waiting for connection! \n");
        new_fd = accept(sockfd, (struct sockaddr*)&client_addr, &sin_size);
        if(new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(client_addr.ss_family, 
            get_in_addr((struct sockaddr*)&client_addr), s, sizeof(s));
        printf("server: got connection from %s\n", s);

        handle_client(new_fd);            

        close(new_fd);
    }
 
	return 0;
}
