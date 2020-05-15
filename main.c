// Starting code for CITS3002 in C

// Headers
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <regex.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>

#define MAXDATASIZE 1028 // Max data size
#define LISTENQ 1 // Max number of client connections

int tcpConnect(int tcpPort) {
    

    return 0;
}

int udpConnect(char *stationName) {

}

int max(int x, int y) {
    if (x>y) return x;
    else return y;
}

int main(int argc, char **argv) {
    
    char *name = argv[1]; // the spoken name of the station that will be used to refer to the station
    int tcpPort = atoi(argv[2]); // port for tcp connection from e.g. http://localhost:port
    int udpPort = atoi(argv[3]); // port for udp for other stations to use to communicate with this station 
    //char **adjacentStations = argv; // we will be using all arguments argv[4:argc]

    tcpConnect(tcpPort);

    int listenfd, udpfd, maxfd, connfd, externalfd, n, nready;
    fd_set rset;
    pid_t childpid;
    socklen_t clientLen;
    char buf[MAXDATASIZE];
    struct sockaddr_in clientAddress, tcpServerAddress, udpServerAddress, externalUdpServerAddress;

    // TCP Setup
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    tcpServerAddress.sin_family = AF_INET;
    tcpServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    tcpServerAddress.sin_port = htons(tcpPort);

    // Binding and listening with TCP
    bind(listenfd, (struct sockaddr *) &tcpServerAddress, sizeof(tcpServerAddress));
    listen(listenfd, LISTENQ);
    
    // UDP Setup
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    udpServerAddress.sin_family = AF_INET;
    udpServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    udpServerAddress.sin_port = htons(udpPort);
    
    // Binding UDP
    bind(udpfd, (struct sockaddr *) &udpServerAddress, sizeof(udpServerAddress));

    printf("%s\n", "Servers running... waiting for connections...");    

    maxfd = max(listenfd, udpfd) + 1;

    // Listen to UDP and TCP
    for(;;) {
        FD_SET(listenfd, &rset);
        FD_SET(udpfd, &rset);

        nready = select(maxfd, &rset, NULL, NULL, NULL);

        if (FD_ISSET(listenfd, &rset)) {
            clientLen = sizeof(clientAddress);
            connfd = accept(listenfd, (struct sockaddr *) &clientAddress, &clientAddress);
            printf("%s\n", "Received request...");
            if((childpid = fork()) == 0) {
                printf("%s\n", "Child created for dealing wih client requests");
                close(listenfd);

                n=recv(connfd, buf, MAXDATASIZE,0);
                printf("%s\n", "String recieved from TCP");
                strtok(buf, "="); // Get rid of everything before the =
                char *destinationStation = strtok(0, " "); // Get rid of everything after the destination name
                
                // Here we become a UDP client after we find the port of the station we are trying to get to

                // First step is to see if its within our adjacent stations (meaning that we need to know the names of those stations as well as the ports)
                // If its not an adjacent port, then we need to find the station, we can do this using some sort of broadcast
                
                int nextPort = 4004; // The port that we found based on the next station in the path towards the destination station
                char *request = "Can you please tell me train time"; // This will be the message that we send to the next station
                externalfd = socket(AF_INET, SOCK_DGRAM, 0); // Need to do error checking on this

                memset(&externalUdpServerAddress, 0, sizeof(externalUdpServerAddress));
                externalUdpServerAddress.sin_family = AF_INET;
                externalUdpServerAddress.sin_port = htons(nextPort);
                externalUdpServerAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

                // Send request to the next server
                sendto(externalfd, request, strlen(request), 0, (struct sockaddr*) &externalUdpServerAddress, sizeof(externalUdpServerAddress));

                // Reply from UDP server (not sure if this will get fed through to the other udpfd section, if so we will need to do some more fanangling)
                recvfrom(externalfd, buf, MAXDATASIZE, 0, (struct sockaddr*) &externalUdpServerAddress, sizeof(externalUdpServerAddress));
                printf("Reply from UDP Request: ");
                puts(buf);
                close(externalfd);

                // Reply to TCP connection
                send(connfd, destinationStation, strlen(destinationStation), 0);
                printf("Replied to TCP Connection. TCP Over.\n");
                
                close(connfd);
                exit(0);
            }
            close(connfd);
        }

        // UDP Server
        if (FD_ISSET(udpfd, &rset)) {
            clientLen = sizeof(clientAddress);
            printf("%s\n", "String recieved from UDP");
            n = recv(udpfd, buf, MAXDATASIZE, 0);
            puts(buf);
            char *reply = "thank you for your message from udp server"
            sendto(udpfd, reply, sizeof(reply), 0, (struct sockaddr*) &clientAddress, sizeof(clientAddress)); 
        }
    }


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

