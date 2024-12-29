#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define BUF_SIZE 4096

void *handle_server_input(void * data);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        return -1;
    }
    char *dest_hostname = argv[1];
    char *dest_port = argv[2];
    int rc;
    int n;
    struct addrinfo hints, *res;

    // Open the Socket:
    int conn_fd;
    if ((conn_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    // Set up hints (must start by setting everything to 0)
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    /*
    DNS LOOKUP!
    &hints: contains what type of address we want to look up
    &res: At end, contains the ip adress for the hostname and port
    */
    if((rc = getaddrinfo(dest_hostname, dest_port, &hints, &res)) != 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    // Connect to the server:
    if(connect(conn_fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        exit(2);
    }
    puts("Connected\n");

    pthread_t child_thread;
    if (pthread_create(&child_thread, NULL, handle_server_input, &conn_fd) != 0) {
        perror("pthread_create");
        return -1;
    }
    
    // Read data from terminal and send to server
    char buf[BUF_SIZE];
    while((n = read(0, buf, BUF_SIZE)) > 0) {
        buf[n] = '\0';
        if (send(conn_fd, buf, n+1, 0) == -1) {
            perror("send");
            return -1;
        }
    }
    puts("Exiting.");
    if (close(conn_fd) == -1) {
        perror("close");
        return -1;
    }
    return 0;
}

void *handle_server_input(void * data) {
    int n;
    char in_buf[BUF_SIZE];
    while((n = recv(*((int *) data), in_buf, BUF_SIZE, 0)) > 0) {
        char out_buff[BUF_SIZE] = {'\0'};
        strncpy(out_buff, in_buf, n);
        puts(out_buff);
    }
    if (n == -1) {
        perror("recv");
    } else {
        puts("Connection Closed by Remote Host.");
    }
    exit(1);
    return NULL;
}