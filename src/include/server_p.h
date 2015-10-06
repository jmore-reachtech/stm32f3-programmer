#ifndef _SERVER_H
#define _SERVER_H

#define TCP_PORT "3490"

#define BACKLOG 10

#define MAX_BUF_LEN 512 

#define ISP_ACK     "ACK"
#define ISP_NACK    "NACK"
#define ISP_INIT    "INIT"
#define ISP_DEINIT  "DEINIT"
#define IPS_UPDATE  "UPDATE"
#define ISP_FW      "FW"
#define ISP_GET_VER "VER"

#define	ISPD_DEFAULT_PORT 7890
#define ISPD_UNIX_SOCKET "/tmp/ispSocket"

int ispd_socket_init(unsigned short port, int *addr_family,
    const char *socket_path);
int ispd_socket_accept(int sfd, int addr_family);

#endif // _SERVER_H
