#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#include "server_p.h"
#include "log.h"

#define DEBUG
#ifdef DEBUG
#define LOG(format, ...) printf(format "\n" , ##__VA_ARGS__);
#else
#define LOG(format, ...)
#endif

static int create_unix_socket(const char*);
static int create_tcp_socket(unsigned short);
static void *get_in_addr(struct sockaddr *);

int ispd_socket_init(unsigned short port, int *addr_family,
    const char *socket_path)
{
    int sfd = -1;

    if (port == 0) {
        sfd = create_unix_socket(socket_path);
        *addr_family = AF_UNIX;
    } else {
        sfd = create_tcp_socket(port);
        *addr_family = AF_INET;
    }
    return sfd;
}

static int create_unix_socket(const char* path)
{
    struct sockaddr_un addr;
    int sfd;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd == -1) {
        log_die_with_system_message("socket()");
    }

    /* Construct server socket address, bind socket to it,
       and make this a listening socket */

    if (remove(path) == -1 && errno != ENOENT) {
        log_die_with_system_message("remove())");
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        log_die_with_system_message("bind()))");
    }

    if (listen(sfd, BACKLOG) == -1) {
        log_die_with_system_message("listen())");
    }

    return sfd;
}

static int create_tcp_socket(unsigned short port)
{
    struct addrinfo hints, *serverinfo, *p;
    int sfd, rv;
    int yes = 1;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, TCP_PORT, &hints, &serverinfo)) != 0) {
        log_die_with_system_message("getaddrinfo())");
    }
    
    for(p = serverinfo; p != NULL; p = p->ai_next) {
        if((sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) {
            log_die_with_system_message("setsockopt()");
        }
        
        if(bind(sfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sfd);
            perror("server: bind");
            continue;
        }
        
        break;
    }

    freeaddrinfo(serverinfo);
    
    if(p == NULL) {
        log_die_with_system_message("server: failed to bind");
    }
 
    if(listen(sfd, BACKLOG) == -1) {
        log_die_with_system_message("server: failed to listen");
    }

    return sfd;
}

int ispd_socket_accept(int sfd, int addr_family)
{
    int cfd;
    socklen_t sin_size; 
    struct sockaddr_storage client_addr;
    char s[INET6_ADDRSTRLEN];

    sin_size = sizeof(client_addr);
    cfd = accept(sfd, (struct sockaddr*)&client_addr, &sin_size);
    
    inet_ntop(client_addr.ss_family, 
        get_in_addr((struct sockaddr*)&client_addr), s, sizeof(s));

    if (cfd >= 0) {
        switch (addr_family) {
        case AF_UNIX:
            log_msg(LOG_INFO, "[ISPD] Handling Unix client\n");
            fprintf(stdout, "[ISPD] Handling Unix client\n");
            break;

        case AF_INET:
            log_msg(LOG_INFO, "[ISPD] Handling TCP client %s\n",s);
            break;

        default:
            break;
        }
    }

    return cfd;
}

/* get sockaddr, IPv4 or IPv6 */
static void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int ispd_socket_read(int sfd, char *msg, size_t buf_size)
{
    int cnt;

    if ((cnt = recv(sfd, msg, buf_size, 0)) <= 0) {
        return -1;
    } else {
        msg[cnt] = '\0';
    }

    return cnt;
}

void ispd_socket_write(int sfd, const char *msg)
{
    int cnt = strlen(msg);
	
    if (send(sfd, msg, cnt, 0) != cnt) {
        perror("what's messed up?");
    }

}
