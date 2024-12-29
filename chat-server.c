#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <stddef.h>

#define BACKLOG 10
#define BUF_SIZE 4096

// Look at everywhere we use the list, and figure out how bad it would be if something else happen

struct client {
    char * username;
    int fd;
    uint16_t rem_port;
    char *rem_ip;
    struct client_list *client_list;
    struct client *next;
    struct client *prev;
};
struct client_list {
    struct client * head;
    struct client * tail;
};
pthread_mutex_t list_mutex;

void get_time(char *t_buff);
void *handle_client(void* data);
void disconect_client(struct client * c);
void send_to_all_clients(struct client * c, char* s, int l);
int rename_client(char * in_buff, char * t_buff, char * out_buff, struct client * c);

int main(int argc, char *argv[])
{
    char *listen_port;
    int listen_fd, conn_fd;
    struct addrinfo hints, *res;
    int rc;
    struct sockaddr_in remote_sa;
    socklen_t addrlen;
    
    struct client_list *cl;
    if ((cl = malloc(sizeof(struct client_list))) == NULL) {
        return -1;
    }
    cl->head = NULL;
    cl->tail = NULL;

    listen_port = argv[1];

    /* create a socket */
    if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }

    /* bind it to a port */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rc = getaddrinfo(NULL, listen_port, &hints, &res)) != 0) {
        printf("getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }
    if (bind(listen_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        return -1;
    }

    /* start listening */
    if (listen(listen_fd, BACKLOG) == -1) {
        perror("listen");
        return -1;
    }

    /* infinite loop of accepting new connections and handling them */
    while(1) {
        /* accept a new connection (will block until one appears) */
        addrlen = sizeof(remote_sa);
        if ((conn_fd = accept(listen_fd, (struct sockaddr *) &remote_sa, &addrlen)) == -1) {
            perror("accept");
            return -1;
        }
 
        /* Set up new client struct and add it to the client list */
        struct client *current_record;
        if ((current_record = malloc(sizeof(struct client))) == NULL) {
            return -1;
        }

        pthread_mutex_lock(&list_mutex);
        if (cl->head == NULL) {
            cl->head = current_record;
        } else {
            cl->tail->next = current_record;
            current_record->prev = cl->tail;
        }
        cl->tail = current_record;
        current_record->client_list = cl;
        current_record->fd = conn_fd;
        current_record->username  = "User unknown";
        current_record->next = NULL;
        char * temp_ip;
        if ((temp_ip = inet_ntoa(remote_sa.sin_addr)) == 0) {
            perror("inet_ntoa");
            pthread_mutex_unlock(&list_mutex);
            return -1;
        }
        if ((current_record->rem_ip = strdup(temp_ip)) == NULL) {
            perror("strdup");
            pthread_mutex_unlock(&list_mutex);
            return -1;
        }
        current_record->rem_port = ntohs(remote_sa.sin_port);
        pthread_mutex_unlock(&list_mutex);

        printf("New connection from %s:%d\n", current_record->rem_ip, current_record->rem_port);
        pthread_t child_thread;
        pthread_create(&child_thread, NULL, handle_client, current_record);
    }
}

void *handle_client(void* data) {
    struct client *c = data;
    int bytes_received;
    while(1) {
        char in_buff[BUF_SIZE] = {'\0'};
        while((bytes_received = recv(c->fd, in_buff, BUF_SIZE, 0)) > 0) {
            int send_bytes = 0;
            char out_buff[BUF_SIZE] = {'\0'};
            char t_buff[10];
            get_time(t_buff);
            if (in_buff[0] == '/') {
                // We may be in the /nick case, check for "nick " (trailing space intentional)
                char command_buff[6] = {'\0'};
                strncpy(command_buff, in_buff + 1, 5);
                if (strcmp(command_buff, "nick ") == 0) {
                    send_bytes = rename_client(in_buff, t_buff, out_buff, c);
                } else {
                    continue;
                }
            } else {
                // Its a message for clients
                int len = bytes_received + 12 + strlen(c->username);
                if ((send_bytes = snprintf(out_buff, len, "%s: %s: %s", t_buff, c->username ,in_buff)) < 0) {
                    perror("snprintf");
                    return NULL;
                }
            }
            send_to_all_clients(c, out_buff, send_bytes);
        }
        if (bytes_received == -1) {
            perror("recv");
            return NULL;
        }
        disconect_client(c);
        return NULL;
    }
}

void get_time(char *t_buff) {
    time_t t;
    struct tm * t_info;
    if (time(&t) == -1) {
        perror("time");
        exit(-1);
    }
    if ((t_info = localtime(&t)) == NULL) {
        perror("localtime");
        exit(-1);
    }
    if (snprintf(t_buff, 9, "%02d:%02d:%02d", t_info->tm_hour, t_info->tm_min, t_info->tm_sec) < 0) {
        perror("snprintf");
        exit(-1);
    }
}

int rename_client(char * in_buff, char * t_buff, char * out_buff, struct client * c) {
    int send_bytes = 0;
    char * name_buff;
    if ((name_buff = malloc(BUF_SIZE)) == NULL) {
        perror("malloc");
        exit(-1);
    }
    int new_name_length = stpncpy(name_buff, (in_buff+6), BUF_SIZE) - name_buff;
    if ((name_buff = realloc(name_buff, new_name_length)) == NULL) {
        perror("realloc");
        exit(-1);
    }
    name_buff[new_name_length-1] = '\0';
    // 78 is total size of string without name
    int len = 78 + new_name_length + strlen(c->username);
    send_bytes = snprintf(out_buff, len, "%s: %s (%s:%d) is now known as %s", t_buff, c->username, c->rem_ip, c->rem_port, name_buff);
    c->username = name_buff;
    return send_bytes;
}

void disconect_client(struct client * c) {
    char t_buff[10];
    get_time(t_buff);
    char disconect_msg[BUF_SIZE];
    // 51 = 15 for time and "User" + 36 for everything else
    int str_len = 52 + strlen(c->username);
    int out_len;
    if ((out_len= snprintf(disconect_msg, str_len, "%s: User %s (%s:%d) has disconnected", t_buff, c->username, c->rem_ip, c->rem_port)) < 0) {
        perror("snprintf");
        exit(-1);
    }
   
    send_to_all_clients(c, disconect_msg, out_len);
    pthread_mutex_lock(&list_mutex);
    /* Update Linked list */
    if (c->client_list->head == c) {
        /* We are removing the HEAD */
        c->client_list->head = c->next;
    } else if (c->client_list->tail == c) {
        /* We are removing the TAIL */
        c->client_list->tail = c->prev;
        c->prev->next = NULL;
    }else{
        /* We are removing any other node */
        c->prev->next = c->next;
        c->next->prev = c->prev;
    }
    pthread_mutex_unlock(&list_mutex);
    printf("Lost connection from %s.\n", c->username);
    fflush(stdout);
    close(c->fd);
    free(c->rem_ip);
    free(c);
}

void send_to_all_clients(struct client * c, char* s, int len) {
    pthread_mutex_lock(&list_mutex);
    struct client *cur = c->client_list->head;
    while (cur->next != NULL) {
        if (send(cur->fd, s, len, 0) == -1) {
            perror("send");
        }
        cur = cur->next;
    }
    if (send(cur->fd, s, len, 0) == -1) {
            perror("send");
            pthread_mutex_unlock(&list_mutex);
            return;
        }
    cur = cur->next;
    pthread_mutex_unlock(&list_mutex);
    return;
}

