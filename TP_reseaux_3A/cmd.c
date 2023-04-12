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


#define PASSMAXLEN 12

struct identity {
    char pseudo[NAMEMAXLEN + 1];
    char password[PASSMAXLEN + 1];
    struct identity *next;
};

struct identity *identities = NULL;

int has_pseudo(client *c) {
    return c->pseudo[0] != '\0';
}

int is_pseudo_used(const char *pseudo) {
    for (client *c = clients; c != NULL; c = c->next) {
        if (strcmp(c->pseudo, pseudo) == 0) {
            return c->sockfd;
        }
    }
    return 0;
}

int set_pseudo(client *cli, const char *pseudo) {
    if (strlen(pseudo) > NAMEMAXLEN)
        return 0;

    if (is_pseudo_used(pseudo))
        return 0;

    strcpy(cli->pseudo, pseudo);
    cli->pseudo[strlen(pseudo)] = '\0';
    printf("Set pseudo: %s(%d)\n", cli->pseudo, cli->sockfd);
    return 1;
}

int is_registered(char *pseudo) {
    for (struct identity *i = identities; i != NULL; i = i->next) {
        if (strcmp(i->pseudo, pseudo) == 0) {
            // it is registered
            return 1;
        }
    }
    return 0;
}

void overwrite_all_registered() {
    FILE *f = fopen("pseudo.dat", "w");
    if (f == NULL) {
        stop("fopen");
    }
    for (struct identity *i = identities; i != NULL; i = i->next) {
        if (fwrite(i, sizeof(struct identity), 1, f) == 0) {
            stop("fwrite");
        }
    }
    fclose(f);
}

int unregister(char *pseudo) {
    if (strlen(pseudo) > NAMEMAXLEN || strlen(pseudo) == 0)
        return 0;

    if (identities != NULL) {
        /// check if it is the first element of the linked list
        if (strcmp(identities->pseudo, pseudo) == 0) {
            struct identity *buf = identities;
            printf("Unregistering %s\n", buf->pseudo);
            identities = identities->next;
            free(buf);
            overwrite_all_registered();  // overwrite to removed the unregistered one
            return 1;
        }
        struct identity *i;
        /// check if it is in the middle of the linked list
        for (i = identities; i->next != NULL; i = i->next) {
            if (strcmp(i->next->pseudo, pseudo) == 0) {
                struct identity *buf = i->next;
                printf("Unregistering %s\n", buf->pseudo);
                i->next = buf->next;
                free(buf);
                overwrite_all_registered();  // overwrite to removed the unregistered one
                return 1;
            }
        }
        /// check if it is the last element of the linked list
        if (strcmp(i->pseudo, pseudo) == 0) {
            struct identity *buf = i->next;
            printf("Unregistering %s\n", buf->pseudo);
            i->next = buf->next;
            free(buf);
            overwrite_all_registered();  // overwrite to removed the unregistered one
            return 1;
        }
    }
    return 0;
}

int register_pseudo(char *pseudo, char *password) {
    if (strlen(pseudo) > NAMEMAXLEN || strlen(password) > PASSMAXLEN
        || strlen(pseudo) == 0 || strlen(password) == 0)
        return 0;

    if (identities != NULL) {
        // linked list is not empty
        struct identity *i;
        for (i = identities; i->next != NULL; i = i->next) {
            if (strcmp(i->pseudo, pseudo) == 0) {
                // pseudo is already registered
                return 0;
            }
        }
        // check last element
        if (strcmp(i->pseudo, pseudo) == 0) {
            // pseudo is already registered
            return 0;
        }
        struct identity *newi = malloc(sizeof(struct identity));
        strcpy(newi->pseudo, pseudo);
        strcpy(newi->password, password);
        newi->next = NULL;
        i->next    = newi;

        FILE *f = fopen("pseudo.dat", "a");
        if (f == NULL) {
            stop("fopen");
        }
        if (fwrite(newi, sizeof(struct identity), 1, f) == 0) {
            stop("fwrite");
        }
        fclose(f);
    } else {
        // linked list is empty
        identities = malloc(sizeof(struct identity));
        strcpy(identities->pseudo, pseudo);
        strcpy(identities->password, password);
        identities->next = NULL;

        FILE *f = fopen("pseudo.dat", "a");
        if (f == NULL) {
            stop("fopen");
        }
        if (fwrite(identities, sizeof(struct identity), 1, f) == 0) {
            stop("fwrite");
        }
        fclose(f);
    }


    return 1;
}

void load_pseudo() {
    FILE *f = fopen("pseudo.dat", "r");
    if (f == NULL) {
        // file does not exists
        return;
    }

    identities= malloc(sizeof(struct identity));
    struct identity *i = identities;
    struct identity buf;
    while (fread(&buf, sizeof(struct identity), 1, f) != 0) {
        strcpy(i->pseudo, buf.pseudo);
        strcpy(i->password, buf.password);
        i->next = malloc(sizeof(struct identity));
        i       = i->next;
    }
    fclose(f);
}

int can_login(char *pseudo, char *password) {
    struct identity *i;
    for (i = identities; i != NULL; i = i->next) {
        if (strcmp(i->pseudo, pseudo) == 0 && strcmp(i->password, password) == 0) {
            return 1;
        }
    }
    return 0;
}

void unset_pseudo(char *pseudo) {
    for (client *c = clients; c != NULL; c = c->next) {
        if (strcmp(c->pseudo, pseudo) == 0) {
            // pseudo was found
            c->pseudo[0] = '\0';
            if (send_as_server(c->sockfd, "Your pseudo has been registered by an other "
                                          "user")
                < 0) {
                stop("send");
            }
        }
    }
}
char *get_word(char **buf) {
    while (isblank(**buf)) {
        (*buf)++;
    }

    if(**buf == '\0') {
        return NULL;
    }

    char *word = *buf;

    while (!isblank(**buf) && **buf != '\0') {
        (*buf)++;
    }

    if(**buf == '\0') {
        ;
    } else {
        **buf = '\0';
        (*buf)++;
    }
    return word;
}

void nickname_pass(client *c, char *pseudo, char *password) {
    // does the pseudo exists ? is the password the right one ?
    int ret = can_login(pseudo, password);

    if (!ret) {
        if (send_as_server(c->sockfd, "Invalid username or password") < 0) {
            stop("send");
        }
        return;
    }

    unset_pseudo(pseudo);   // remove this pseudo from everyone
    set_pseudo(c, pseudo);  // assign it to the right client
    if (send_as_server(c->sockfd, "Pseudo changed") < 0) {
        stop("send");
    }
}

void nickname_no_pass(client *c, char *pseudo) {
    int ret = is_registered(pseudo);  // is the pseudo registered ?
    if (ret) {
        if (send_as_server(c->sockfd, "Pseudo already registered") < 0) {
            stop("send");
        }
        return;
    }

    ret = set_pseudo(c, pseudo);  // try to set it to the client
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

void nickname(client *c, char *buf) {
    /// get the pseudo
    char *startname = get_word(&buf);
    if(!startname) {
        if (send_as_server(c->sockfd, "No pseudo provided") < 0) {
            stop("send");
        }
        return;
    }

    /// get the password
    char *password = get_word(&buf);
    if (!password) {
        // no password
        nickname_no_pass(c, startname);
    } else {
        // password
        nickname_pass(c, startname, password);
    }
}

void register_command(client *c, char *buf) {
    /// get the pseudo
    char *pseudo = get_word(&buf);
    if(!pseudo) {
        if (send_as_server(c->sockfd, "No pseudo provided") < 0) {
            stop("send");
        }
        return;
    }

    /// get the password
    char *password = get_word(&buf);
    if(!password) {
        if (send_as_server(c->sockfd, "No password provided") < 0) {
            stop("send");
        }
        return;
    }

    if (register_pseudo(pseudo, password)) {
        send_as_server(c->sockfd, "Pseudo registered");
    } else {
        send_as_server(c->sockfd, "Registering failed");
    }
}

void unregister_command(client *c, char *buf) {
    /// get the pseudo
    char *pseudo = get_word(&buf);
    if(!pseudo) {
        if (send_as_server(c->sockfd, "No pseudo provided") < 0) {
            stop("send");
        }
        return;
    }

    /// get the password
    char *password = get_word(&buf);
    if(!password) {
        if (send_as_server(c->sockfd, "No password provided") < 0) {
            stop("send");
        }
        return;
    }

    if (!is_registered(pseudo)) {
        if (send_as_server(c->sockfd, "Pseudo not registered") < 0) {
            stop("send");
        }
        return;
    }

    if (!can_login(pseudo, password)) {
        if (send_as_server(c->sockfd, "Wrong password") < 0) {
            stop("send");
        }
        return;
    }
    if (unregister(pseudo)) {
        if (send_as_server(c->sockfd, "Pseudo unregistered") < 0) {
            stop("send");
        }
    } else {
        // everything was checked before, this should never be executed
        if (send_as_server(c->sockfd, "Unknown error") < 0) {
            stop("send");
        }
    }
}

void exit_command(client *c) {
    if (send_as_server(c->sockfd, "Disconnected") < 0) {
        stop("send");
    }
    printf("Disconnection: %s(%d)\n", c->pseudo, c->sockfd);
    remove_all_channel_from(c);
    // remove after because it closes the sockfd
    remove_from_clients(c);
}

void date_command(client *c) {
    uint64_t time = gettime();
    if (send_as_server(c->sockfd, ctime((time_t *)&time)) < 0) {
        stop("send");
    }
}

void mp_command(client *c, char *buf) {
    /// get the pseudo
    char *pseudo = get_word(&buf);
    if(!pseudo) {
        if (send_as_server(c->sockfd, "No pseudo provided") < 0) {
            stop("send");
        }
        return;
    }

    if (strlen(buf) == 0) {
        if (send_as_server(c->sockfd, "No message provided") < 0) {
            stop("send");
        }
        return;
    }

    int dest_sockfd;
    if (!(dest_sockfd = is_pseudo_used(pseudo))) {
        if (send_as_server(c->sockfd, "Pseudo does not exists") < 0) {
            stop("send");
        }
        return;
    }

    if (send_as_to(c, dest_sockfd, buf, FLAG_PRIVATE) < 0) {
        stop("send");
    }
}


void alerte_command(client *c, char *buf) {
    /// get the pseudo
    char *pseudo = get_word(&buf);

    int dest_sockfd;
    if (pseudo && (dest_sockfd = is_pseudo_used(pseudo))) {
        --buf;  // there is at least "/alerte pseudo" in the buffer
                // more than enough to go back a few characters

        *buf = '\a';  // bell character

        *--buf        = 'm';     //
        *--buf        = '1';     // bold
        *--buf        = '[';     //
        *--buf        = '\x1B';  //
        uint8_t flags = FLAG_BG_NORMAL |  FLAG_PRIVATE;
        if (send_as_to(c, dest_sockfd, buf, flags) < 0) {
            stop("send");
        }
    } else {
        if(pseudo) {
            // no pseudo = no word = no message = no strlen(NULL) because it is UB
            int len = strlen(pseudo);
            *buf    = ' ';  // replace '\0' by ' '
            buf -= len;     // go back to before the username that is not one
            // send all
        }
        --buf;        // there is at least "/alerte " in the buffer
                      // more than enough to go back a few characters
        *buf = '\a';  // bell character

        *--buf = 'm';     //
        *--buf = '1';     // bold
        *--buf = '[';     //
        *--buf = '\x1B';  //

        uint8_t flags = FLAG_BG_NORMAL ;
        send_all_but(c, buf, flags);
    }
}

void join_command(client *c, char *args) {
    /// get the server name
    char *name = get_word(&args);
    if(!name) {
        if (send_as_server(c->sockfd, "No server name provided") < 0) {
            stop("send");
        }
        return;
    }

    if (!join_channel(c, name)) {
        // should never be executed
        if (send_as_server(c->sockfd, "Could not join the server") < 0) {
            stop("send");
        }
    }
    return;
}

void command(client *c, char *buf) {
    if (strncmp(buf, "nickname ", 9) == 0) {
        nickname(c, buf + 9);
    } else if (strncmp(buf, "register ", 9) == 0) {
        register_command(c, buf + 9);
    } else if (strncmp(buf, "unregister ", 11) == 0) {
        unregister_command(c, buf + 11);
    } else if (strncmp(buf, "exit", 4) == 0) {
        exit_command(c);
    } else if (strncmp(buf, "date", 4) == 0) {
        date_command(c);
    } else if (strncmp(buf, "mp ", 3) == 0) {
        mp_command(c, buf + 3);
    } else if (strncmp(buf, "alerte ", 7) == 0) {
        alerte_command(c, buf + 7);
    } else if (strncmp(buf, "join ", 5) == 0) {
        join_command(c, buf + 5);
    } else {
        printf("Command does not exist\n");
        if (send_as_server(c->sockfd, "Command does not exist") < 0) {
            stop("send");
        }
    }
}