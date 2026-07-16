#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 500
#define CMD_SIZE 52 // 52 bytes for 26 int16_t values

union CMD_DATA {
    unsigned char bytes[CMD_SIZE];
    int16_t values[CMD_SIZE/2];
};

/**
 * @brief: initializes UDP system and sets the socket file descriptor
 * @param: IP address 
 * @param: port 
 * @return: 0 on success, -1 on failure
 */

int UDP_init(char *ip, char *port);

int UDP_send(union CMD_DATA data);
