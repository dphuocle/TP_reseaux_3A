#include "client.h"

#include "irc.h"

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

#define RECVBUFSIZE \
    (sizeof(uint8_t) + (NAMEMAXLEN + 1) + (CHANMAXLEN + 1) + sizeof(uint64_t) + BUFSIZE)

/// see header associated
char buf[RECVBUFSIZE];
struct sockaddr_in serv_addr;
int sockfd;

void stop(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}
void send_file(char *args) {
    int abort = 0;
    while (isblank(*args) && *args != '\0') {  // skip spaces between command and pseudo
        args++;
    }
    while (!isblank(*args) && *args != '\0') {  // skip pseudo
        args++;
    }
    if (*args == '\0') {
        printf("No file specified\n");
        // abort file sending
        abort = 1;
    }
    while (isblank(*args) && *args != '\0') {  // skip spaces between pseudo and filename
        args++;
    }
    if (strlen(args) == 0) {
        printf("No file specified\n");
        // abort file sending
        abort = 1;
    }

    FILE *f = fopen(args, "r");
    if (f == NULL) {
        printf("Can not open\n");
        abort = 1;
    }

    // we can reuse 'buf' as all arguments were sent to the server
    if (recv(sockfd, buf, RECVBUFSIZE - 1, 0) <= 0) {
        stop("send");
    }

    if (strncmp(MESSAGE(buf), "send_start", 10) != 0) {
        char time[10];
        strftime(time, 10, "%H:%M", localtime((time_t *)TIMESTAMP(buf)));
        printf("[%10s](%s) %s: %s%s\n",
                CHANNEL(buf), time, SENDER(buf), MESSAGE(buf),
               RESET_FORMAT);
        return;
    }

    if(abort) {
        // abort file sending
        if (send(sockfd, "abort", 6, 0) < 0) {
            stop("send");
        }
        return;
    }

    // start file sending
    if (send(sockfd, "start", 6, 0) < 0) {
        stop("send");
    }
    // read in the file and send to client
    int size;
    while ((size = fread(buf, 1, BUFSIZE - 1, f)) == BUFSIZE - 1) {
        buf[size] = '\0';
        printf("sending: %dB\n", size);
        if (send(sockfd, buf, size, 0) < 0) {
            stop("send");
        }
    }
    buf[size] = '\0';
    if (send(sockfd, buf, size, 0) < 0) {
        stop("send");
    }
}

void receive_file(int sockfd, char *args) {
    int write_or_print = 1;    // 1 is write to file, 0 is print to screen
    while (!isblank(*args)) {  // skip command
        args++;
    }

    /// get the filename
    while (isblank(*args)) {
        args++;
    }
    char *filename = args;

    while (!isblank(*args) && *args != '\0') {
        args++;
    }

    *args = '\0';

    printf("Downloading %s", filename);  // no '\n' to send on one line later as we don't
                                         // have the sender's pseudo yet

    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        printf("Can not open file for download\n");
        write_or_print = 0;
    }

    int valread;
 
    while ((valread = recv(sockfd, buf, RECVBUFSIZE - 1, 0)) == RECVBUFSIZE - 1) {
        buf[valread] = '\0';
        if (write_or_print)
            fwrite(MESSAGE(buf), 1, strlen(MESSAGE(buf)), f);
        else
            printf("%s", buf);
    }
    if (valread != 0) {
        buf[valread] = '\0';
        if (write_or_print)
            fwrite(MESSAGE(buf), 1, strlen(MESSAGE(buf)), f);
        else
            printf("%s", buf);
    }
    printf(" sent by %s\n", SENDER(buf));
    fclose(f);
}

int main(int argc, char **argv) {
    // the arguments must be the ip address and the connection port
    if (argc != 3) {
        stop("2 arguments required");
    }

    bzero(buf, BUFSIZE);
    // create the socket to the server
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        stop("socket");
    }

    bzero(&serv_addr, sizeof(struct sockaddr_in));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port        = htons(atoi(argv[2]));

    // connect to the socket to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
        stop("connect");
    }

    // 'You joined a new server', we don't want that, just discard it
    if (recv(sockfd, buf, BUFSIZE - 1, 0) < 0) {
        stop("recv");
    }

    // get the pseudo
    do {
        printf("username: ");
        fflush(stdout);
        scanf("%511s", buf);
        buf[511] = '\0';
        if (strlen(buf) > NAMEMAXLEN) {
            // pseudo too long
            printf("(shorter than %d characters) ", NAMEMAXLEN);
            continue;
        }

        // send to server for confirmation
        if (send(sockfd, buf, strlen(buf), 0) < 0) {
            stop("send");
        }

        // read server answer
        int valread = recv(sockfd, buf, BUFSIZE - 1, 0);
        if (valread < 0) {
            stop("recv");
        }
        // if pseudo was autorised the answer will be "Pseudo changed"
    } while (strncmp(MESSAGE(buf), "Pseudo changed", 14) != 0);

    // send hello to everyone
    if (send(sockfd, "Hello", 6, 0) < 0) {
        stop("send");
    }

    fd_set readfds;
    int max_sd = STDIN_FILENO ? STDIN_FILENO > sockfd : sockfd;

    for (;;) {
        // set readfds for the server and stdin (for input)
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            stop("select");
        }
        // if message from server, we shall read it, format it and print it
        if (FD_ISSET(sockfd, &readfds)) {
            int valread
              = recv(sockfd, buf,
                     sizeof(uint8_t) + (NAMEMAXLEN + 1) + sizeof(uint64_t) + BUFSIZE - 1, 0);
            if (valread <= 0) {
                // valread = 0 means server crashed
                stop("recv");
            }
            buf[valread] = '\0';
            char *pseudo = SENDER(buf);
            if (IS_SERVER(buf) && strncmp(MESSAGE(buf), "Disconnected", 12) == 0) {
                // exit
                break;
            }
            if (IS_SERVER(buf) && strncmp(MESSAGE(buf), "/send ", 6) == 0) {
                receive_file(sockfd, MESSAGE(buf));
            } else {
                char time[10];
                strftime(time, 10, "%H:%M", localtime((time_t *)TIMESTAMP(buf)));
                if (IS_PRIVATE(buf) && !IS_SERVER(buf)) {
                    printf("(%s) private message from %s: %s%s\n",
                             time,pseudo, MESSAGE(buf), RESET_FORMAT);
                } else {
                    printf("[%10s](%s) %s: %s%s\n",
                           CHANNEL(buf), time, pseudo,
                           MESSAGE(buf), RESET_FORMAT);
                }
            }
        } else if (FD_ISSET(STDIN_FILENO, &readfds)) {
            // if message from stdin, we shall read it and send it to the server
            /*scanf("%511s", buf);*/
            char c      = '\0';
            char *input = buf;
            while ((c = getchar()) != '\n') {
                if (c == EOF) {
                    stop("getchar");
                }
                *input = c;
                input++;
                if (input - buf > BUFSIZE - 1) {
                    // buffer is full
                    // if we try to send a file whith a name longer than ~500 chars
                    // it won't work but that's unlikely
                    *input = '\0';
                    if (send(sockfd, buf, strlen(buf), 0) < 0) {
                        stop("send");
                    }
                    input = buf;
                }
            }
            *input = '\0';
            if (send(sockfd, buf, strlen(buf), 0) < 0) {
                stop("send");
            }
            if (strncmp(buf, "/send ", 6) == 0) {
                send_file(buf + 6);
            }
        }
    }
    close(sockfd);
    return EXIT_SUCCESS;
}