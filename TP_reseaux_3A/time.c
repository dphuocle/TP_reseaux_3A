#include "irc.h"
#include "server.h"

#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SET_LI(packet, li)     (uint8_t)(packet.li_vn_mode |= (li << 6))
#define SET_VN(packet, vn)     (uint8_t)(packet.li_vn_mode |= (vn << 3))
#define SET_MODE(packet, mode) (uint8_t)(packet.li_vn_mode |= (mode << 0))
#define NTP_TIMESTAMP_DELTA    2208988800ull

typedef struct {
    uint8_t li_vn_mode;  // Eight bits. li, vn, and mode.
                         // li.   Two bits.   Leap indicator.
                         // vn.   Three bits. Version number of the protocol.
                         // mode. Three bits. Client will pick mode 3 for client.

    uint8_t stratum;    // Eight bits. Stratum level of the local clock.
    uint8_t poll;       // Eight bits. Maximum interval between successive messages.
    uint8_t precision;  // Eight bits. Precision of the local clock.

    uint32_t rootDelay;       // 32 bits. Total round trip delay time.
    uint32_t rootDispersion;  // 32 bits. Max error aloud from primary clock source.
    uint32_t refId;           // 32 bits. Reference clock identifier.

    uint32_t refTm_s;  // 32 bits. Reference time-stamp seconds.
    uint32_t refTm_f;  // 32 bits. Reference time-stamp fraction of a second.

    uint32_t origTm_s;  // 32 bits. Originate time-stamp seconds.
    uint32_t origTm_f;  // 32 bits. Originate time-stamp fraction of a second.

    uint32_t rxTm_s;  // 32 bits. Received time-stamp seconds.
    uint32_t rxTm_f;  // 32 bits. Received time-stamp fraction of a second.

    uint32_t txTm_s;  // 32 bits and the most important field the client cares
                      // about. Transmit time-stamp seconds.
    uint32_t txTm_f;  // 32 bits. Transmit time-stamp fraction of a second.

} ntp_packet;  // Total: 384 bits or 48 bytes.

uint64_t gettime() {
    // setup the ntp packet
    ntp_packet packet;
    bzero(&packet, sizeof(ntp_packet));
    SET_LI(packet, 0);
    SET_VN(packet, 3);
    SET_MODE(packet, 3);

    int sockfd;
    // create the socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        stop("socket");
    }

    // get server address
    struct hostent *server = gethostbyname("pool.ntp.org");
    if (server == NULL) {
        // shitty insa wifi
        /*stop("gethostbyname");*/
        return 0;
    }

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    bcopy(server->h_addr_list[0], &serv_addr.sin_addr.s_addr, sizeof(unsigned long));
    serv_addr.sin_port = htons(1230);

    // connect to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        stop("connect");
    }

    // send the packet asking for time
    if (sendto(sockfd, (char *)&packet, sizeof(ntp_packet), 0,
               (struct sockaddr *)&serv_addr, sizeof(serv_addr))
        < 0) {
        stop("sendto");
    }


    fd_set readfds;
    int max_sd;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    max_sd       = sockfd;

    // timeval of 10ms
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 10000;

    // wait for the answer but only for 10ms
    int activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);
    if (activity < 0) {
        stop("select");
    }
    if (activity == 0) {
        // timed out: I don't know the date, give the one of the server instead
        uint64_t timestamp;
        time((time_t *)&timestamp);
        return timestamp;
    }

    unsigned sz = sizeof(serv_addr);
    if (recvfrom(sockfd, (char *)&packet, sizeof(ntp_packet), 0,
                 (struct sockaddr *)&serv_addr, (socklen_t *)&sz)
        < 0) {
        perror("recvfrom");
        printf("Can not fetch date\n");
        return 0;
    }

    // convert the time
    packet.txTm_s      = ntohl(packet.txTm_s);
    uint64_t timestamp = packet.txTm_s - NTP_TIMESTAMP_DELTA;
    /*printf("%s\n", ctime((time_t *)&timestamp));*/

    return timestamp;
}
