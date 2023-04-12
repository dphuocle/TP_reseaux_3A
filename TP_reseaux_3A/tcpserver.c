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

void stop(const char *message) {
    perror(message);
    exit(1);
}
int main(int argc, char* argv[]) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int newsockfd;
    socklen_t clilen;
    char buffer[BUFSIZE];
    int n;

    if(sockfd == -1) {
        stop("Error opening socket");
        exit(errno);
    }

    struct sockaddr_in sockaddr, cli_addr;
    // exercice 4 
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(1234);
    sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // exercice 5 
    if (bind(sockfd, (struct sockaddr*) &sockaddr, sizeof(sockaddr)) < 0){
        stop("ERROR on binding");
    }

    // exercice 6
    int queue_client = 5;
    if(listen(sockfd,queue_client) < 0) {
        stop("ERROR on listen");
    }

    // exercice 7
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);

    if (newsockfd < 0){
        stop("ERROR on accept");
    }

    // exercice 8
    // Boucle de 1000 itérations
    for (int i = 0; i < 1000; i++)
    {
        // Réception du message du client
        memset(buffer, 0, BUFSIZE);
        n = recv(newsockfd, buffer, BUFSIZE, MSG_WAITALL);
        if (n < 0)
        {
            stop("ERROR reading from socket");
        }
        printf("Message recu : %s\n", buffer);

        // Envoi du message au client
        n = send(newsockfd, buffer, strlen(buffer), MSG_OOB);
        if (n < 0)
        {
            stop("ERROR writing to socket");     
        }
    }

    // Fermeture de la connexion
    close(newsockfd);
    close(sockfd);

    return 0;
}