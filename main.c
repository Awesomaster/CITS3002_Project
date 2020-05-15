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

#define MAXDATASIZE 100


int main(int argc, char **argv) {
    char *name = argv[1]; // the spoken name of the station that will be used to refer to the station
    char *tcpPort = argv[2]; // port for tcp connection from e.g. http://localhost:port
    //int udpPort = argv[3]; // port for udp for other stations to use to communicate with this station 

    //char **adjacentStations = argv; // we will be using all arguments argv[4:argc]



    
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(NULL, tcpPort, &hints, &res);

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    bind(sockfd, res->a-_addr, res->ai_addrlen);
    

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