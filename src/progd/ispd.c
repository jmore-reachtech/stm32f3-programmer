#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include "common_p.h"
#include "server_p.h"
#include "log.h"
#include "serial.h"
#include "gpio.h"
#include "stm32.h"


struct socket_status {
    int server_fd;
    int client_fd;
    int addr_family;
    int port;
    char *socket_path;
};

struct serial_port_status {
    int fd;
    struct serial_port_options opts;
};

static struct isp_status{
    int running;
    struct socket_status sock_status;
    struct serial_port_status sport_status;
} isp_status = {
    .running = 0,
    .sock_status = {
        .server_fd      = 0,
        .client_fd      = 0,
        .addr_family    = 0,
        .port           = 0,
        .socket_path    = ISPD_UNIX_SOCKET, 
    },
    .sport_status = {
        .fd = 0,
        .opts = {
            .device     = TTY_DEV,
            .baud_rate  = 57600,
        },
    },
};

static int handle_client(int cfd)
{
    printf("server: sending 3 bytes '%s'\n", ISP_ACK);
    if(send(cfd, ISP_ACK, 3 ,0) == -1) {
        perror("send");
    }

    return 0;
}

void ispd_sig_handler(int sig)
{
    fprintf(stdout, "got sig TERM\n");
    isp_status.running = 0;
}

int main(int argc, char **argv)
{
    fd_set cur_fdSet; 
    struct isp_status *status = &isp_status;
    struct socket_status *sock = &(isp_status).sock_status;
    struct serial_port_status *sport = &(isp_status).sport_status;

    {
        /* install a signal handler to remove the socket file */
        struct sigaction a;
        memset(&a, 0, sizeof(a));
        a.sa_handler = ispd_sig_handler;
        if (sigaction(SIGINT, &a, 0) != 0) {
            log_die_with_system_message("[ISPD] sigaction(SIGINT) failed,");
        }
        if (sigaction(SIGTERM, &a, 0) != 0) {
            log_die_with_system_message("[ISPD] sigaction(SIGTERM) failed, ");
        }
    }

    if((sport->fd = serial_init(&(sport->opts))) < 0) {
        log_die_with_system_message("serial init failed");
    }
    fprintf(stdout, "serial fd %d\n", sport->fd);

    if((sock->server_fd = ispd_socket_init(0, &(sock->addr_family),  
                                            sock->socket_path)) < 0) {
        log_die_with_system_message("socket init failed");
    }
    fprintf(stdout, "socket fd %d\n", sock->server_fd);
    
    FD_ZERO(&cur_fdSet);
    FD_SET(sock->server_fd, &cur_fdSet);

    status->running = 1;
    while(status->running) {
        if((sock->client_fd = ispd_socket_accept(sock->server_fd, 
                                                    sock->addr_family)) < 0) {
            log_die_with_system_message("socket accept failed");
        }
    
        handle_client(sock->client_fd);            

        close(sock->client_fd);
    }

    fprintf(stdout, "closing up shop\n");
    if(sock->server_fd) {
        close(sock->server_fd);
    }
    
    if(sport->fd) {
        serial_deinit(&(sport->opts));
    }
    
    if(sock->port == 0) {
        /* best effort removal of socket */
        const int rv = unlink(sock->socket_path);
        if (rv == 0) {
            log_msg(LOG_INFO, "[ISPD] socket file %s unlinked\n", 
                sock->socket_path);
        } else {
            log_msg(LOG_INFO, "[IPD] socket file %s unlink failed\n", 
                sock->socket_path);
        }
    }
 
	return 0;
}
