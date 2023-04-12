#include "server.h"

#include "irc.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


char buf[BUFSIZE];

char sendbuf[sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1) + sizeof(uint64_t) + BUFSIZE];

#define PORT 8000

client *clients = NULL;

void stop(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void print_client() {
    for (client *c = clients; c != NULL; c = c->next) {
        printf("%p: %s, %p\n", c, c->pseudo, c->next);
    }
}

int send_as_server(int sockfd, char *msg) {
    uint8_t flags = 0x03;  // private message from server
    memcpy(sendbuf, &flags, 1);
    memcpy(sendbuf + sizeof(uint8_t), "\\server", 8);  // server's pseudo
    memcpy(sendbuf + sizeof(uint8_t) + (NAMEMAXLEN + 1), "\\server", 8);  // server's
                                                                          // channel
    uint64_t time = gettime();
    memcpy(sendbuf + sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1), &time,
           sizeof(uint64_t));  // timestamp
    memcpy(sendbuf + sizeof(uint8_t) + sizeof(uint64_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1),
           msg, strlen(msg));  // message

    return send(
      sockfd, sendbuf,
      sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1) + sizeof(uint64_t) + strlen(msg), 0);
}

int send_as_to(client *c, int sockfd, char *msg, uint8_t flags) {
    memcpy(sendbuf, &flags, 1);
    memcpy(sendbuf + sizeof(uint8_t), c->pseudo, NAMEMAXLEN + 1);  // pseudo
    memcpy(sendbuf + sizeof(uint8_t) + (NAMEMAXLEN + 1), c->current_channel, CHANMAXLEN + 1);  // channel
    uint64_t time = gettime();
    memcpy(sendbuf + sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1), &time,
           sizeof(uint64_t));  // timestamp
    memcpy(sendbuf + sizeof(uint8_t) + sizeof(uint64_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1),
           msg, strlen(msg));  // message

    return send(
      sockfd, sendbuf,
      sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1) + sizeof(uint64_t) + strlen(msg), 0);
}

void remove_from_clients(client *cr) {
    for (client *c = clients; c != NULL; c = c->next) {
        if (c->next == cr) {
            // client found
            c->next = cr->next;
            close(cr->sockfd);
            free(cr->addr);
            free(cr);
        }
    }
}

void add_to_clients(int fd, struct sockaddr_in *addr) {
    for (client *c = clients; c != NULL; c = c->next) {
        if (c->next == NULL) {
            // at the end
            client *new_client = malloc(sizeof(client));
            if (new_client == NULL) {
                stop("malloc");
            }
            new_client->pseudo[0] = '\0';
            new_client->sockfd    = fd;
            new_client->next      = NULL;
            new_client->addr      = addr;
            new_client->channels  = NULL;
            c->next               = new_client;
            join_channel(new_client, "general");
            printf("atc: %s : %p : %p\n", c->next->channels->name, c->next->channels->owner, c->next->channels->next);
            break;
        }
    }
}
void send_all_but(client *cli, const char *str, uint8_t flags) {
    for (client *c = clients->next; c != NULL; c = c->next) {
        if (c->sockfd != cli->sockfd && has_pseudo(c) && has_channel(c, cli->current_channel)) {
            // a valid client we want to send to
            memcpy(sendbuf, &flags, 1);
            memcpy(sendbuf + sizeof(uint8_t), cli->pseudo, NAMEMAXLEN + 1);
            memcpy(sendbuf + sizeof(uint8_t) + (NAMEMAXLEN + 1), cli->current_channel, CHANMAXLEN + 1);
            uint64_t time = gettime();
            memcpy(sendbuf + sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1), &time,
                   sizeof(uint64_t));
            memcpy(sendbuf + sizeof(uint8_t) + sizeof(uint64_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1),
                   str, strlen(str));
            if (sendto(c->sockfd, sendbuf,
                       sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1)
                         + sizeof(uint64_t) + strlen(str),
                       0, (struct sockaddr *)c->addr, sizeof(*c->addr))
                < 0) {
                stop("sendto");
            }
        }
    }
}


int main(int argc, char **argv) {
    init_channel();
    load_pseudo();
    bzero(buf, BUFSIZE);
    // create the master socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        stop("socket");
    }

    // reuse port so we don't need to wait 2min
    int options = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &options, sizeof(options)) < 0) {
        stop("setsockopt");
    }

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(struct sockaddr_in));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // localhost
    serv_addr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        stop("bind");
    }

    if (listen(sockfd, 5) < 0) {
        stop("listen");
    }

    // the first client is the server
    client *self = malloc(sizeof(client));
    if (self == NULL) {
        stop("malloc");
    }
    self->pseudo[0] = '\0';
    self->current_channel[0] = '\0';
    self->channels = NULL;
    self->sockfd = sockfd;
    self->next   = NULL;
    clients      = self;

    fd_set readfds;
    int max_sd;

    printf("Waiting for connections...\n");
    for (;;) {
        FD_ZERO(&readfds);  // clear FDs
        FD_SET(sockfd, &readfds);
        max_sd = sockfd;

        // set all the sockfds
        for (client *c = clients; c != NULL; c = c->next) {
            FD_SET(c->sockfd, &readfds);
            if (c->sockfd > max_sd)
                max_sd = c->sockfd;
        }


        // wait for activity
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            stop("select");
        }

        /*print_channel();*/

        // a connection was initialized, accept it and create a new client for it
        if (FD_ISSET(sockfd, &readfds)) {
            int new_sockfd;
            struct sockaddr_in *new_sockaddr = malloc(sizeof(struct sockaddr_in));
            int new_addrlen                  = sizeof(struct sockaddr_in);
            if ((new_sockfd = accept(sockfd, (struct sockaddr *)new_sockaddr, (socklen_t *)&new_addrlen))
                < 0) {
                stop("accept");
            }

            add_to_clients(new_sockfd, new_sockaddr);

            printf("New connection: %d\n", new_sockfd);
        }

        client *pc = NULL;  // previous client to handle disconnection
        // a message was sent by a connected client
        for (client *c = clients->next; c != NULL; pc = c, c = c->next) {
            if (FD_ISSET(c->sockfd, &readfds)) {
                int valread = read(c->sockfd, buf, BUFSIZE - 1);
                if (valread < 0) {
                    stop("read");
                } else if (valread == 0) {
                    // the client disconnected
                    printf("Disconnection: %s(%d)\n", c->pseudo, c->sockfd);
                    remove_from_clients(c);
                    c = pc;
                    if (c == NULL) {
                        break;
                    }
                } else {
                    // the client sent a message
                    buf[valread] = '\0';
                    printf("Message from %s(%d): %s\n", c->pseudo, c->sockfd, buf);
                    if (!has_pseudo(c)) {
                        // client does not have a pseudo, it wants one, try to attribute
                        // it
                        if (is_registered(buf)) {
                            if (send_as_server(c->sockfd, "Pseudo already registered") < 0) {
                                stop("send");
                            }
                        } else {
                            int ret = set_pseudo(c, buf);
                            if (ret) {
                                if (send_as_server(c->sockfd, "Pseudo changed") < 0) {
                                    stop("send");
                                }
                            } else {
                                if (send_as_server(c->sockfd, "Pseudo already in use") < 0) {
                                    stop("send");
                                }
                            }
                        }
                    } else if (buf[0] == '/') {
                        // message starts with a '\', it is a command
                        command(c, buf + 1);
                    } else {
                        // it is just a regular message, forward it to everyone else
                        send_all_but(c, buf, 0);
                    }
                }
            }
        }
    }
    return EXIT_SUCCESS;
}