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

/* Uncomment for full debugging */
//#define DEBUG
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
static void cmd_version(void);
static int cmd_update(void);
static void ispd_notify_client(ispd_notify_t msg);
static void cmd_quit(void);

/* 
    Structure for socket info 
*/
struct socket_status {
    int server_fd;
    int client_fd;
    int addr_family;
    int port;
    char *socket_path;
};

/*
    Structure for STM32 info
*/
struct micro_status {
    stm32_state_t micro_state;
    char *fw_path;
    int ver_major;
    int ver_minor;
    int ver_patch;
};

/*
    Structure for current work state
*/
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
        .fw_path        = "/home/root/main.bin",
        .ver_major      = 0,
        .ver_minor      = 0,
        .ver_patch      = 0,
    },
};

/*
    These messages map to Qml item object names. We use these 
    to set the current status.
*/
const char *messages[] = {
    "txtStatus.text=Ready\n",
    "txtStatus.text=Busy\n",
    "txtStatus.text=Idle\n",
    "txtStatus.text=Updating\n",
    "txtStatus.text=Complete\n",
};

/*
    We use a simple command scheme. Qml sends us 3 bytes 'MV\n'.
    We only care about the second byte to know what to do. Here
    we look for a valid command and if found handle it.
*/
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
        case 'Q':
            LOG("Quit cmd");
            cmd = MQ;
            break;
        default:
            LOG("Invalid cmd");
            cmd = IV;
    }

    handle_cmd(cmd);
}

/*
    Here we handle a valid command.
*/
static void handle_cmd(ispd_cmd_t cmd)
{
    LOG("%s", __func__);
    switch(cmd) {
        case MS:
            micro_init();
            break;
        case MV:
            cmd_version();
            break;
        case MU:
            cmd_update();
            break;
        case MQ:
            cmd_quit();
            break;
        default:
            return;
    }
}

/* 
    Reset the STM32. To put the STM32 in reset pull the boot pin high, 
    and toggle the reset pin. This will bring the STM32 up in bootloader mode.
    To return the STM32 to normal, pull the boot pin low and toggle the reset pin.
*/
static void reset_micro(pin_state s)
{
    LOG("%s %d", __func__, s);
	gpio_toggle_boot(s);
	sleep(1);
	gpio_toggle_reset(LOW);
	sleep(1);
	gpio_toggle_reset(HIGH);
	sleep(1);
}

/* 
    Set up the STM32 in bootloader mode. Here we init the GPIO port 
    and send the STM32 an init byte. 
*/
static void micro_init(void)
{
    LOG("%s", __func__);
	if(gpio_init() != 0) {
	    LOG("gpio init failed!");
	}
	reset_micro(HIGH);

	if(stm_init_seq(&(isp_status.sport_opts)) != 0) {
        isp_status.m_status.micro_state = STM32_FAILED;
    }

    LOG("STM32_READY");
    isp_status.m_status.micro_state = STM32_READY;
}

/* 
    Reset the STM32 to normal running state and reset the GPIO.
*/
static void micro_deinit(void)
{
    LOG("%s", __func__);
	reset_micro(LOW);
	gpio_deinit();
    isp_status.m_status.micro_state = STM32_IDLE;
}

/*
    Quit command, Qml is done talking to the STM32 so reset the micro
    and GPIO. We also clear the running flag and send an IDLE status 
    message to Qml.
*/
static void cmd_quit(void)
{
    LOG("%s", __func__);
    micro_deinit();
    isp_status.m_status.micro_state = STM32_IDLE;
    isp_status.running = 0;
    ispd_notify_client(MSG_IDLE);
}

/*
    Version command, read the application version from the STM32 and 
    notify Qml.
*/
static void cmd_version(void)
{
	uint8_t data[4] = {0};
	uint32_t addr = 0x0;
    char ver[32];

    LOG("%s", __func__);
	if((stm_read_mem(&(isp_status).sport_opts, USER_DATA_OFFSET, data, 4)) != 0) {
        isp_status.m_status.micro_state = STM32_FAILED;
        goto err;
    }

	addr = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0] << 0;
    
    LOG("%d.%d.%d \n", VERSION_MAJOR(addr), VERSION_MINOR(addr), 
        VERSION_PATCH(addr));
    
    isp_status.m_status.ver_major = VERSION_MAJOR(addr);
    isp_status.m_status.ver_minor = VERSION_MINOR(addr);
    isp_status.m_status.ver_patch = VERSION_PATCH(addr);

    /* This message goes to Qml to display the version  */
    sprintf(ver,"micro_input.text=%d.%d.%d\n", 
            isp_status.m_status.ver_major,
            isp_status.m_status.ver_minor,
            isp_status.m_status.ver_patch);
    ispd_socket_write(isp_status.sock_status.client_fd, ver);

err:
    return;
}

/* 
    Update command, update the firmware on the STM32. This reads a file 
    from the filesystem and writes it to the STM32. The STM32 accepts 
    256 bytes for each write so we divide the file size by 256 to give 
    us an ops count and use this to notify Qml of the progress.
*/
static int cmd_update(void)
{
	FILE *fp;
	ssize_t r;
	long size;
	uint8_t tmp[MAX_RW_SIZE];
	uint32_t addr = STM_FLASH_BASE;
	unsigned int i;
    unsigned int num_ops;
    int ret;
    char msg[32];

    /* Notify Qml we are updating */
    ispd_notify_client(MSG_UPDATING);
	fp = fopen(isp_status.m_status.fw_path, "rb");
	if(fp == NULL) {
		LOG("File '%s' not found!", isp_status.m_status.fw_path);
		return 1;
	}

	fseek (fp , 0 , SEEK_END);
	size = ftell (fp);
	rewind (fp);

    /* How many 256 byte chunks we have */
    num_ops = size / MAX_RW_SIZE;
    
	LOG("%s: file size is %ld; ops = %d. fw = %s\n", __func__,
            size,
            num_ops,
            isp_status.m_status.fw_path);

    /* Need to erase before we update */
	if(stm_erase_mem(&(isp_status).sport_opts) != 0) {
        return 1;
	}

	r = fread (tmp,1,MAX_RW_SIZE,fp);
	while (r > 0) {
		if(r < MAX_RW_SIZE) {
			LOG("\n%s: padding buffer, read %d", __func__, r);
			for(i = r; i < MAX_RW_SIZE; i++) {
				tmp[i] = 0xFF;
			}
		}
		LOG("\n%s: writing %d bytes to flash", __func__, MAX_RW_SIZE);
		ret = stm_write_mem(&(isp_status).sport_opts, addr,tmp, MAX_RW_SIZE);
		addr += r;

        /* Update the Qml status element */
        sprintf(msg,"txtStatus.text=%d\n", num_ops--);
        ispd_socket_write(isp_status.sock_status.client_fd, msg);

		r = fread (tmp,1,MAX_RW_SIZE,fp);
	}

	fclose (fp);
	
    /* Notify Qml the update is complete */
    ispd_notify_client(MSG_COMPLETE);
	return 0;
}

/*
    Helper function to send a message to Qml
*/
static void ispd_notify_client(ispd_notify_t msg)
{
    ispd_socket_write(isp_status.sock_status.client_fd, 
        messages[msg]);
}

/*
    Signal handler for SIGINT and SIGTERM. Clear the running flag, 
    this allows the main processing loop to fall through.
*/
void ispd_sig_handler(int sig)
{
    LOG("got sig TERM");
    isp_status.running = 0;
}

/*
    Application entry point
*/
int main(int argc, char **argv)
{
    fd_set cur_fdset; 
    int nfds = 0;
    /* pointers to nested structures, for ease of access */
    struct isp_status *status = &isp_status;
    struct socket_status *sock = &(isp_status).sock_status;
    struct serial_port_options *sport = &(isp_status).sport_opts;
    struct micro_status *micro = &(isp_status).m_status;

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

    /* Init the serial port */
    if((sport->fd = serial_init(sport)) < 0) {
        log_die_with_system_message("serial init failed");
    }

    /* Expose a server socket for Qml */
    if((sock->server_fd = ispd_socket_init(0, &(sock->addr_family),  
                                            sock->socket_path)) < 0) {
        log_die_with_system_message("socket init failed");
    }

    /* 
        We really don't need a select set but set one up anyway, we may use it 
        down the road. 
    */
    FD_ZERO(&cur_fdset);
    FD_SET(sock->server_fd, &cur_fdset);

    /* We need the highest file descriptor for the select call */
    nfds = MAX(sock->server_fd, sport->fd);

    /* set our running flag */
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
            /* We have a new client connection, clear the server fd from
                the select set and add the client fd */
            FD_CLR(sock->server_fd, &cur_fdset);
            FD_SET(sock->client_fd, &cur_fdset);
            nfds = MAX(sock->client_fd, sport->fd);
            LOG("New connection, say Hello");
            /* Notify Qml we are here and ready to go */
            ispd_notify_client(MSG_READY);
        }

        /* check for packet received on the client socket */
        if((sock->client_fd >= 0) && (FD_ISSET(sock->client_fd, &read_fdset))) {
            char msg_buf[CMD_SIZE];
            read_count = ispd_socket_read(sock->client_fd, msg_buf,
                                                    sizeof(msg_buf));
            if (read_count < 0) {
                LOG("Client closed socket");
                /* The client is gone. Clear that fd from the select set 
                    and add back the server fd so we can listen for a new
                    connection. We may want to see what state the STM32 is in
                    and react accordingly */
                FD_CLR(sock->client_fd, &cur_fdset);
                FD_SET(sock->server_fd, &cur_fdset);
                nfds = MAX(sock->server_fd, sport->fd);
                sock->client_fd = -1;
            } else if (read_count > 0) {
                LOG("Client socket has data, %d bytes", read_count);
                /* A valid command from QMl is 3 bytes with the last byte being
                    a newline (0xA). */
                if(read_count == CMD_SIZE && msg_buf[CMD_SIZE-1] == CMD_EOL) {
                    /* We have a valid command, process it */
                    process_cmd(msg_buf);
                }
            }
 
        }
    }

    /* We are going down, clean up! */
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
    
    /* We are using the same unix socket name as the TIO Agent! Make 
        sure we remove the file so the TIO Agent can start */
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
