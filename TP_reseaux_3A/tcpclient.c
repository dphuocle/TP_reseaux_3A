#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

void stop(const char *s) {
    perror(s);
    exit(1);
}
int main(int argc, char* argv[]) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sockaddr;
    char buffer[BUFSIZE];
    int i;

    if(sockfd == -1) {
        stop("Error opening socket");
        exit(errno);
    }

    // exercice 14 
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(1234);
    sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //Connect to remote server
    if (connect(sockfd, (struct sockaddr*) &sockaddr , sizeof(sockaddr)) < 0)
    {
        perror("Connect failed. Error");
        return 1;
    }

    // exercice 15
    // Boucle de 1000 itérations
    for(i = 0; i < 1000; i++) {
        char message[5] = "ECHO";
        send(sockfd, message, strlen(message) + 1, MSG_OOB);

        // Attendez la reception d'un message
        int n = recv(sockfd, buffer, sizeof(buffer), 0);
        if (n < 0) {
            perror("ERROR receive the message");
            return 1;
        }
        printf("Reponse recu : %s\n", buffer);
    }
    // Fermeture de la connexion
    close(sockfd);

    return 0;
}