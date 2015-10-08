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

#define DEBUG
#ifdef DEBUG
#define LOG(format, ...) printf(format "\n" , ##__VA_ARGS__);
#else
#define LOG(format, ...)
#endif

static void process_cmd(char *buf);
static void handle_cmd(ispd_cmd_t cmd);
static void reset_micro(pin_state s);
static void micro_init(void);
static void micro_deinit(void);
static int say_hello(int cfd);

struct socket_status {
    int server_fd;
    int client_fd;
    int addr_family;
    int port;
    char *socket_path;
};

struct micro_status {
    stm32_state_t micro_state; 
    int ver;
};

static struct isp_status{
    int running;
    struct socket_status sock_status;
    struct serial_port_options sport_opts;
    struct micro_status m_status;
} isp_status = {
    .running        = 0,
    .sock_status = {
        .server_fd      = 0,
        .client_fd      = 0,
        .addr_family    = 0,
        .port           = 0,
        .socket_path    = ISPD_UNIX_SOCKET, 
    },
    .sport_opts = {
        .fd         = 0,
        .device     = TTY_DEV,
        .baud_rate  = 57600,
    },
    .m_status = {
        .micro_state    = STM32_IDLE,
        .ver            = 0,
    },
};

static void process_cmd(char *buf)
{
    ispd_cmd_t cmd = IV;

    switch (buf[1]) {
        case 'S':
            LOG("Start cmd");
            cmd = MS;
            break;
        case 'V':
            LOG("Version cmd");
            cmd = MV;
            break;
        case 'U':
            LOG("Update cmd");
            cmd = MU;
            break;
        case 'G':
            LOG("Go cmd");
            cmd = MG;
            break;
        default:
            LOG("Invalid cmd");
            cmd = IV;
    }

    handle_cmd(cmd);
}

static void handle_cmd(ispd_cmd_t cmd)
{
    LOG("%s", __func__);
    switch(cmd) {
        case MS:
            micro_init();
            break;
        default:
            return;
    }
}

static void reset_micro(pin_state s)
{
    LOG("%s", __func__);
	gpio_toggle_boot(s);
	sleep(1);
	gpio_toggle_reset(LOW);
	sleep(1);
	gpio_toggle_reset(HIGH);
	sleep(1);
}

static void micro_init(void)
{
    LOG("%s", __func__);
    gpio_init();
	reset_micro(HIGH);

	if(stm_init_seq(&(isp_status.sport_opts)) != 0) {
        isp_status.m_status.micro_state = STM32_FAILED;
    }

    LOG("STM32_READY");
    isp_status.m_status.micro_state = STM32_READY;
}

static void micro_deinit(void)
{
    LOG("%s", __func__);
	reset_micro(LOW);
	gpio_deinit();
    isp_status.m_status.micro_state = STM32_IDLE;
}

static int say_hello(int cfd)
{
    const char *msg = "micro_input.text=1\n";

    LOG("server: sending %d bytes", strlen(msg));
    if(send(cfd, msg, strlen(msg) ,0) == -1) {
        perror("send");
    }

    return 0;
}

void ispd_sig_handler(int sig)
{
    LOG("got sig TERM");
    isp_status.running = 0;
}

int main(int argc, char **argv)
{
    fd_set cur_fdset; 
    int nfds = 0;
    /* pointers to nested structures */
    struct isp_status *status = &isp_status;
    struct socket_status *sock = &(isp_status).sock_status;
    struct serial_port_options *sport = &(isp_status).sport_opts;
    struct micro_status *micro = &(isp_status).m_status;
    char serial_buf[MAX_BUF_LEN];

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

    if((sport->fd = serial_init(sport)) < 0) {
        log_die_with_system_message("serial init failed");
    }

    if((sock->server_fd = ispd_socket_init(0, &(sock->addr_family),  
                                            sock->socket_path)) < 0) {
        log_die_with_system_message("socket init failed");
    }
    
    FD_ZERO(&cur_fdset);
    FD_SET(sock->server_fd, &cur_fdset);
    FD_SET(sport->fd, &cur_fdset);
    
    nfds = MAX(sock->server_fd, sport->fd);

    status->running = 1;
    while(status->running) {
        fd_set read_fdset = cur_fdset;
        int read_count = 0;
        
        LOG("Waiting for someone to blink");
        /* wait indefinitely for someone to blink */
        const int sel = select(nfds+1, &read_fdset, 0, 0, 0);

        if (sel == -1) {
            if (errno == EINTR) {
                LOG("Select returned EINTR");
                break;  /* drop out of inner while */
            } else {
                log_die_with_system_message("select() returned -1");
            }
        } else if (sel <= 0) {
            continue;
        }

        /* check for a new connection to accept */
        if(FD_ISSET(sock->server_fd, &read_fdset)) {
            if((sock->client_fd = ispd_socket_accept(sock->server_fd, 
                                                    sock->addr_family)) < 0) {
                log_die_with_system_message("socket accept failed");
            }
            FD_CLR(sock->server_fd, &cur_fdset);
            FD_SET(sock->client_fd, &cur_fdset);
            nfds = MAX(sock->client_fd, sport->fd);
            LOG("New connection, say Hello");
            say_hello(sock->client_fd);
        }

        /* check for packet received on the client socket */
        if((sock->client_fd >= 0) && (FD_ISSET(sock->client_fd, &read_fdset))) {
            char msg_buf[CMD_SIZE];
            read_count = ispd_socket_read(sock->client_fd, msg_buf,
                                                    sizeof(msg_buf));
            if (read_count < 0) {
                LOG("Client closed socket");
                FD_CLR(sock->client_fd, &cur_fdset);
                FD_SET(sock->server_fd, &cur_fdset);
                nfds = MAX(sock->server_fd, sport->fd);
                sock->client_fd = -1;
            } else if (read_count > 0) {
                LOG("Client socket has data, %d bytes", read_count);
                if(read_count == CMD_SIZE && msg_buf[CMD_SIZE-1] == CMD_EOL) {
                    process_cmd(msg_buf);
                }
            }
 
        }
 
       /* check for a character on the serial port */
       if(FD_ISSET(sport->fd, &read_fdset)) {
            read_count = serial_read(sport, serial_buf, 0);
            LOG("TTY has data, %d bytes", read_count);
            if (read_count < 0) {
                /* the serial port died, get out */
                log_die_with_system_message("serial port read failed");
            } else {
                ispd_socket_write(sock->client_fd, serial_buf);
            }
       } 
    }

    LOG("closing up shop");
    if(micro->micro_state == STM32_READY) {
        micro_deinit();
    }    

    if(sock->server_fd) {
        close(sock->server_fd);
    }
    if(sock->client_fd) {
        close(sock->client_fd);
    }
    
    if(sport->fd) {
        serial_deinit(sport);
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
