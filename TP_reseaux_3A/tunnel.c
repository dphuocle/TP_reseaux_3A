#include "irc.h"
#include "server.h"

#include <arpa/inet.h>
#include <ctype.h>
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


channel *channels;



void init_channel() {
    channels = malloc(sizeof(channel));
    strcpy(channels->name, "general");
    channels->owner = NULL;
    channels->next  = NULL;
}


void print_channel() {
    printf("Channels:\n");
    for (channel *c = channels; c != NULL; c = c->next) {
        printf("\t%p : %s owned by %p : %p\n", c, c->name, c->owner, c->next);
    }
}


void print_channel_client() {
    for (client *cli = clients; cli != NULL; cli = cli->next) {
        printf("%p : %s in %s : %p\n", cli, cli->pseudo, cli->current_channel, cli->next);
        for (channel *c = cli->channels; c != NULL; c = c->next) {
            printf("\t%s : %p : %p\n", c->name, c->owner, c->next);
        }
    }
}


client *is_channel(const char *name) {
    for (channel *c = channels; c != NULL; c = c->next) {
        if (strcmp(c->name, name) == 0) {
            // channel already in use
            return c->owner;
        }
    }
    return (void *)-1;
}
int has_channel(client *c, char *name) {
    for (channel *chan = c->channels; chan != NULL; chan = chan->next) {
        if (strcmp(chan->name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

int create_channel(char *name, client *owner) {
    if (strlen(name) > CHANMAXLEN || strlen(name) == 0)
        return 0;

    channel *c;
    for (c = channels; c->next != NULL; c = c->next) {
        if (strcmp(c->name, name) == 0) {
            // channel already exists
            return 0;
        }
    }
    // check last element
    if (strcmp(c->name, name) == 0) {
        // channel is already registered
        return 0;
    }
    channel *newc = malloc(sizeof(channel));
    strcpy(newc->name, name);
    newc->name[CHANMAXLEN] = '\0';
    newc->owner            = owner;
    newc->next             = NULL;
    c->next                = newc;
    printf("Channel %s created by %s\n", newc->name, newc->owner->pseudo);

    return 1;
}
int remove_from_channel(char *pseudo, char *name) {
    for (client *c = clients; c != NULL; c = c->next) {
        if (strcmp(pseudo, c->pseudo) == 0) {
            /// check if it is the first element of the linked list
            if (strcmp(c->channels->name, name) == 0) {
                printf("%s quitting channel %s\n", c->pseudo, name);
                if (strcmp(name, c->current_channel) == 0) {
                    join_channel(c, "general");
                    if (send_as_server(c->sockfd, "Your were removed from this channel "
                                                  "and "
                                                  "sent back to 'general'")
                        < 0) {
                        stop("send");
                    }
                }
                free(c->channels);
                c->channels = NULL;
                return 1;
            }
            channel *chan;
            /// check if it is in the middle of the linked list
            for (chan = c->channels; chan->next != NULL; chan = chan->next) {
                if (strcmp(chan->next->name, name) == 0) {
                    channel *buf = chan->next;
                    printf("%s quitting channel %s\n", c->pseudo, name);
                    chan->next = buf->next;
                    if (strcmp(name, c->current_channel) == 0) {
                        join_channel(c, "general");
                        if (send_as_server(c->sockfd, "Your were removed from this "
                                                      "channel and "
                                                      "sent back to 'general'")
                            < 0) {
                            stop("send");
                        }
                    }
                    free(buf);
                    return 1;
                }
            }
            /// check if it is the last element of the linked list
            if (strcmp(chan->name, name) == 0) {
                channel *buf = chan->next;
                printf("%s quitting channel %s\n", c->pseudo, name);
                chan->next = buf->next;
                if (strcmp(name, c->current_channel) == 0) {
                    join_channel(c, "general");
                    if (send_as_server(c->sockfd, "Your were removed from this channel "
                                                  "and "
                                                  "sent back to 'general'")
                        < 0) {
                        stop("send");
                    }
                }
                free(buf);
                return 1;
            }
        }
    }
    return 0;
}

int remove_all_from_channel(char *name) {
    int res = 1;
    for (client *c = clients; c != NULL; c = c->next) {
        if (c->channels == NULL)
            continue;
        /// check if it is the first element of the linked list
        if (strcmp(c->channels->name, name) == 0) {
            printf("%s quitting channel %s\n", c->pseudo, name);
            if (strcmp(name, c->current_channel) == 0) {
                printf("aa\n");
                join_channel(c, "general");
                if (send_as_server(c->sockfd, "Your were removed from this channel and "
                                              "sent back to 'general'")
                    < 0) {
                    stop("send");
                }
            }
            free(c->channels);
            c->channels = NULL;
            res &= 1;
            continue;
        }
        int this_left = 0;
        /// check if it is in the middle of the linked list
        channel *chan;
        for (chan = c->channels; chan->next != NULL; chan = chan->next) {
            if (strcmp(chan->next->name, name) == 0) {
                channel *buf = chan->next;
                printf("%s quitting channel %s\n", c->pseudo, name);
                chan->next = buf->next;
                if (strcmp(name, c->current_channel) == 0) {
                    join_channel(c, "general");
                    if (send_as_server(c->sockfd, "Your were removed from this channel "
                                                  "and "
                                                  "sent back to 'general'")
                        < 0) {
                        stop("send");
                    }
                }
                free(buf);
                res &= 1;
                this_left = 1;
                break;
            }
        }
        if(this_left)
            continue;
        /// check if it is the last element of the linked list
        if (strcmp(chan->name, name) == 0) {
            channel *buf = chan->next;
            printf("%s quitting channel %s\n", c->pseudo, name);
            chan->next = buf->next;
            if (strcmp(name, c->current_channel) == 0) {
                join_channel(c, "general");
                if (send_as_server(c->sockfd, "Your were removed from this channel and "
                                              "sent back to 'general'")
                    < 0) {
                    stop("send");
                }
            }
            free(buf);
            res &= 1;
            continue;
        }
    }
    return res;
}

int delete_channel(char *name) {
    if (strlen(name) > CHANMAXLEN || strlen(name) == 0)
        return 0;

    channel *c;
    /// check if it is in the middle of the linked list
    for (c = channels; c->next != NULL; c = c->next) {
        if (strcmp(c->next->name, name) == 0) {
            channel *buf = c->next;
            printf("Deleting channel %s\n", buf->name);
            c->next = buf->next;
            return 1;
        }
    }
    /// check if it is the last element of the linked list
    if (strcmp(c->name, name) == 0) {
        channel *buf = c->next;
        printf("Deleting channel %s\n", buf->name);
        c->next = buf->next;
        return 1;
    }
    return 0;
}

int remove_all_channel_from(client *c) {
    int res      = 1;
    channel *buf = c->channels;
    c->channels  = NULL;
    for (channel *chan = buf; chan != NULL; chan = buf) {
        buf = chan->next;
        if (chan->owner == c) {
            /*printf("owner of %s\n", chan->name);*/
            remove_all_from_channel(chan->name);
            res &= delete_channel(chan->name);
        } else {
            free(chan);
        }
    }
    return res;
}

int add_channel_to(client *c, char *name) {
    channel *chan;
    if (c->channels == NULL) {
        c->channels = malloc(sizeof(channel));
        chan        = c->channels;
    } else {
        for (chan = c->channels; chan->next != NULL; chan = chan->next) {
        }
        chan->next = malloc(sizeof(channel));
        chan       = chan->next;
    }
    client *o;
    if ((o = is_channel(name)) != (void *)-1) {
        chan->owner = o;
    } else {
        if (!create_channel(name, c)) {
            return 0;
        }
        chan->owner = c;
    }
    strcpy(chan->name, name);
    chan->name[CHANMAXLEN] = '\0';
    chan->next             = NULL;
    /*printf("act: %s : %p : %p\n", chan->name, chan->owner, chan->next);*/
    return 1;
}

int join_channel(client *c, char *name) {
    if (strlen(name) > CHANMAXLEN || strlen(name) == 0)
        return 0;

    strcpy(c->current_channel, name);
    /*print_channel_client();*/
    for (channel *chan = c->channels; chan != NULL; chan = chan->next) {
        if (strcmp(chan->name, name) == 0) {
            if (send_as_server(c->sockfd, "You joined a new channel") < 0) {
                stop("send");
            }
            return 1;
        }
    }
    int res = add_channel_to(c, name);
    printf("%s joins channel %s\n", c->pseudo, name);
    if (send_as_server(c->sockfd, "You joined a new channel") < 0) {
        stop("send");
    }
    return res;
}