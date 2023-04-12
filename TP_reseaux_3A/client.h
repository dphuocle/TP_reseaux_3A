#ifndef CLIENT_H
#define CLIENT_H

#include "irc.h"

#define BUFSIZE 512

extern struct sockaddr_in serv_addr;  
extern int sockfd;                   


extern char buf[sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1) + sizeof(uint64_t) + BUFSIZE];  

#define FLAGS(buf)      (*(uint8_t *)buf)
#define IS_PRIVATE(buf) (FLAGS(buf) & 0x01)
#define IS_SERVER(buf)  (FLAGS(buf) & 0x02)
#define SENDER(buf)     (buf + sizeof(uint8_t))
#define CHANNEL(buf)    (buf + sizeof(uint8_t) + (NAMEMAXLEN + 1))
#define TIMESTAMP(buf) \
    ((uint64_t *)(buf + sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1)))
#define MESSAGE(buf) \
    (buf + sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1) + sizeof(uint64_t))
#define RESET_FORMAT "\x1B[0m"


void stop(char *msg);


#endif  // CLIENT_H
