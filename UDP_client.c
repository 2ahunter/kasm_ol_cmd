#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "UDP_client.h"

int UDP_fd=0;

int UDP_init(char *ip, char *port){

    int sfd; // socket file descriptor
    int s; //
    struct addrinfo  hints;
    struct addrinfo  *result, *rp; // pointers to linked list of addrinfo structures

        /* Obtain address(es) matching host/port. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // Datagram socket 
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          // Any protocol 

    s = getaddrinfo(ip, port, &hints, &result);
    if (s < 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return(s);
    }

        /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  // Success

        close(sfd);
    }

    freeaddrinfo(result);           // No longer needed
    if (rp == NULL) {               // No address succeeded
        fprintf(stderr, "Could not connect\n");
        return(-1);
    }

    //set send and receive timeouts for the socket
    struct timeval timeout;      
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    
    s = setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                sizeof timeout);
    if (s < 0){
        fprintf(stderr, "Failed to set socket options: %s\n", gai_strerror(s));
        return(s);
    }

    s = setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                sizeof timeout);
    if (s < 0){
        fprintf(stderr, "Failed to set socket options: %s\n", gai_strerror(s));
        return(s);
    }

    UDP_fd = sfd; // success! set module level file descriptor
    return(sfd);
}


int UDP_send(union CMD_DATA data){
    ssize_t len = CMD_SIZE;
    ssize_t sent = 0;

    sent = send(UDP_fd, data.bytes, len, 0);
    if (sent != len) {
        fprintf(stderr, "partial/failed write\n");
    }
    return(sent);
}



#ifdef UDP_TESTING
int main(int argc, char *argv[])
{
    int fd; // socket file descriptor returned by UDP_init()
    union CMD_DATA cmd_data, buf_data;
    ssize_t bytes_sent;
    char *ip_addr;
    char *port;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s host port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ip_addr = argv[1];
    port = argv[2];
    fd = UDP_init(ip_addr, port);

    if(fd<0){
        fprintf(stderr, "Failed to get socket descriptor");
        exit(EXIT_FAILURE);
    }

    // populate the buffer with random values
    for(int i = 0; i < CMD_SIZE/2; i++){
        int16_t value = (rand() % 0xFFFF)>>8; // generate small random values
        cmd_data.values[i] = value;
        buf_data.values[i] = htons(value);
    }

    /* print the values*/
    for(int i = 0; i < CMD_SIZE/2; i++){
        printf("Value %d: %d\n", i, cmd_data.values[i]);
    }

    /* send command buffer values */
    bytes_sent = UDP_send(buf_data);

    // printf("Sent %zu bytes\n", bytes_sent);
    exit(EXIT_SUCCESS);
}
#endif