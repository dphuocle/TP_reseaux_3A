#ifndef SERVER_H
#define SERVER_H

#include "irc.h"

#define BUFSIZE 512
extern client *clients;
extern char buf[BUFSIZE];

void stop(char *msg);
void remove_from_clients(client *cr);
void command(client *c, char *buf);
int has_pseudo(client *c);
int set_pseudo(client *cli, const char *pseudo);
int send_as_server(int sockfd, char *msg);
int is_registered(char *pseudo);
int register_pseudo(char *pseudo, char *password);
int can_login(char *pseudo, char *password);
void unset_pseudo(char *pseudo);
int unregister(char *pseudo);
int is_pseudo_used(const char *pseudo);
int send_as_to(client *c, int sockfd, char *msg, uint8_t flags);
void send_all_but(client *cli, const char *str, uint8_t flags);
void load_pseudo();
void init_channel();
int join_channel(client *c, char *name);
int remove_from_channel(char *pseudo, char *name);
client *is_channel(const char *name);
int has_channel(client *c, char *name);
void print_channel();
void print_client();
int remove_all_channel_from(client *c);

#endif  // SERVER_H
