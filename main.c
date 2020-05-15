// Starting code for CITS3002 in C

// Headers
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <arpd/inet.h>

#include <netinet/in.h>

#define MAXDATASIZE 1028


int main(int argc, char **argv) {
    char *name = argv[1]; // the spoken name of the station that will be used to refer to the station
    char *tcpPort = argv[2]; // port for tcp connection from e.g. http://localhost:port
    //int udpPort = argv[3]; // port for udp for other stations to use to communicate with this station 

    //char **adjacentStations = argv; // we will be using all arguments argv[4:argc]


    struct sockaddr_in serverAddress;
    int sockfd;
    char sendLine[MAXDATASIZE]l, recvLine[MAXDATASIZE];

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol); // Make sure this returns 0 (error check)

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_flags = hton(tcpPort); // Here we are performing a host to network transition of the port number for big-endianness

    connect(sockfd, (struct sockaddr *) &serverAddress, sizeof(serverAddress));

    while(fgets(sendline, strlen(sendline), 0)) {
        send(sockfd, sendLine, strlen(sendLine), 0);

        if(recv(sockfd, recvLine, MAXDATASIZE, 0) == 0) {
            perror("The server terminated :P");
            exit(4);
        } 
        printf("%s %s", name, "recived string from server: ");
        fputs(recvLine, stdout);

    }

    exit(0);
    
    /* pseudo code
    if (station in adjacentStations) {
        return nextTrip
    } else {
        findStation (by looking at adjacent stations)
        addNewStationsFound (to address book)
        findRoute to station
        return stepsOfRoute
    }

    */
}