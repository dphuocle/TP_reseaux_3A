#ifndef IRC_H
#define IRC_H

#include <stdint.h>

#define NAMEMAXLEN 10

#define CHANMAXLEN 10

struct client;

typedef struct channel {
    char name[CHANMAXLEN + 1];
    struct client *owner;
    struct channel *next;
} channel;

// representing a client connected to the server
typedef struct client {
    char pseudo[NAMEMAXLEN + 1];
    char current_channel[CHANMAXLEN + 1];
    int sockfd;
    channel *channels;
    struct sockaddr_in *addr;
    struct client *next;
} client;

uint64_t gettime();


#define FG_NORMAL  "\x1B[39m"
#define BG_NORMAL  "\x1B[49m"
#define FLAG_FG_NORMAL  0x00
#define FLAG_BG_NORMAL  0x00

#define FLAG_PRIVATE 0x01

#endif  // IRC_H
