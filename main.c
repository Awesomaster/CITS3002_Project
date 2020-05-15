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

#define MAXDATASIZE 4096 // Max data size
#define MAXADJACENT 20 // Max amount of adjacent stations
#define LISTENQ 4 // Max number of client connections

struct station {
    char name[20];
    int port;
}

char* udpSend(int stationPort, char *message) {
    struct sockaddr_in udpOutAddress;
    int udpOutfd;
    char buf[MAXDATASIZE];

    if ((udpOutfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed UDP socket");
        exit(EXIT_FAILURE);
    }

    memset(&udpOutAddress, 0, sizeof(udpOutAddress));
    udpOutAddress.sin_family = AF_INET;
    udpOutAddress.sin_port = htons(stationPort);
    udpOutAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Send request to the stationPort
    printf("Sending %s request to: %i\n", message, stationPort);
    fflush(stdout);
    sendto(udpOutfd, message, strlen(message), 0, (struct sockaddr*) &udpOutAddress, sizeof(udpOutAddress));

    /*
    printf("Waiting for response from port %i", stationPort);
    
    // Reply from UDP server (not sure if this will get fed through to the other udpfd section, if so we will need to do some more fanangling)
    bzero(buf, sizeof(buf));
    recvfrom(udpOutfd, buf, sizeof(buf), 0, (struct sockaddr*) &udpOutAddress, sizeof(udpOutAddress));
    printf("Reply from UDP Request: ");
    puts(buf);
    */
    close(udpOutfd);

    return buf;
}

int broadcast(int *adjacentPorts, char *message) {
    for (int i=0; i<sizeof(adjacentPorts); i++) { // NOTE: Might need to check the sizeof adjacentStations (might need to div by sizeof(int))
        if(adjacentPorts[i] != 0) {
            printf(udpSend(adjacentPorts[i], message));
        }
    }
    return 0;
}

int max(int x, int y) {
    if (x>y) return x;
    else return y;
}

int main(int argc, char **argv) {
    printf("Code is starting\n");
    // Variables defined from argv
    char *name = argv[1]; // the spoken name of the station that will be used to refer to the station
    int tcpPort = atoi(argv[2]); // port for tcp connection from e.g. http://localhost:port
    int udpPort = atoi(argv[3]); // port for udp for other stations to use to communicate with this station 
    
    int adjacentPorts[MAXADJACENT];
    int position = 0;
    bzero(adjacentPorts, sizeof(adjacentPorts));

    while (argv[position+4][0] != '?') {
        adjacentPorts[position] = atoi(argv[position+4]);
        position++;
    } 

    struct station stationArray[sizeof(adjacentPorts)];
    int dictionaryPosition = 0;

    // Variabled required for socket connections
    int listenfd, udpfd, maxfd, connfd, externalfd, n, nready;
    fd_set rset;
    pid_t childpid;
    socklen_t clientLen;
    char buf[MAXDATASIZE];
    struct sockaddr_in clientAddress, tcpServerAddress, udpServerAddress, externalUdpServerAddress;

    // Regex variables
    regex_t regex;
    int regexCheck = 0;

    // TCP Setup
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&tcpServerAddress, sizeof(tcpServerAddress));
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

    // This is to find the name of all adjacent stations
    char broadcastMessage[50];
    strcpy(broadcastMessage, "NAME:");
    strcat(broadcastMessage, name);
    strcat(broadcastMessage, ":");
    strcat(broadcastMessage, argv[3]);
    broadcast(adjacentPorts, broadcastMessage);

    FD_ZERO(&rset);
    maxfd = max(listenfd, udpfd) + 1;

    // Listen to UDP and TCP
    printf("Listening on ports\n");
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

                bzero(buf, sizeof(buf));
                recv(connfd, buf, sizeof(buf),0);
                printf("%s\n", "String recieved from TCP");
                strtok(buf, "="); // Get rid of everything before the =
                char *destinationStation = strtok(0, " "); // Get rid of everything after the destination name
                char *time = "currentTime";

                char *finalResult = ""; // This will be the time of arrival and path to destination

                // Here we become a UDP client after we find the port of the station we are trying to get to
                char request[100];
                strcpy(request, "PATH:");
                strcat(request, destinationStation);
                strcat(request, time); // This will be the message that we send to the next station
                
                broadcast(adjacentPorts, request); // We ask around for if anyone knows how to get to destination

                // Reply to TCP connection
                send(connfd, finalResult, strlen(finalResult), 0); // Send time of arrival and path of destination back to the web
                printf("Replied to TCP Connection. TCP Over.\n");
                
                close(connfd);
                exit(0);
            }
            close(connfd);
        }

        // UDP Server
        if (FD_ISSET(udpfd, &rset)) {
            printf("%s", "String recieved from UDP: ");
            bzero(buf, sizeof(buf));
            recvfrom(udpfd, buf, MAXDATASIZE, 0, (struct sockaddr*) &clientAddress, sizeof(clientAddress));
            puts(buf);

            // Check the request (using regex)
            regexCheck = regcomp(&regex, "NAME", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); // This will check if "NAME" as a string exists within the UDP message, meaning that they want to know our name 
            char sendingName[50];
            
            strcpy(sendingName, "IAM:");
            strcat(sendingName, name);
            strcat(sendingName, ":");
            strcat(sendingName, argv[3]);
            if (regexCheck == 0) { // They are asking for our name
                printf("Regex Check Success: IAM\n");
                strtok(buf,":"); // This is required to get to the next tokens 

                // We can add this to our dictionary, meaning by the time all the stations have been created and performed the broadcast, every station will have a complete dictionary of adjacent stations names and ports
                char returnName[20];
                strcpy(returnName, strtok(0,":"));
                int returnPort = atoi(strtok(0,":"));
                
                /*
                stationArray[dictionaryPosition].name = returnName;
                stationArray[dictionaryPosition].port = returnPort;
                dictionaryPosition++;
                */

                udpSend(returnPort, sendingName);
                //sendto(connfd, sendingName, strlen(sendingName), 0, (struct sockaddr*) &clientAddress, sizeof(clientAddress)); 
                printf("Replied to NAME request with: %s\n", sendingName);
            } 
            
            regexCheck = regcomp(&regex, "PATH:", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); // This will check if "PATH" as a string exists within the UDP message, meaning that they want to know how to get somewhere
            if (regexCheck == 0) { // They are asking for the path to their destination
                printf("Regex Check Success: PATH\n");
                char *reply = "idk yet";
                sendto(connfd, reply, strlen(reply), 0, (struct sockaddr*) &clientAddress, sizeof(clientAddress)); 
            
            } 

            regexCheck = regcomp(&regex, "IAM:", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); // This will be a reply
            if (regexCheck == 0) { // This is a reply meaning that they have given us their name
                printf("Regex Check Success: NAME\n");
                printf("This was a name\n");

                /*
                char incomingName[20];
                strcpy(incomingName, strtok(0,":"));
                int incomingPort = atoi(strtok(0,":"));
                stationArray[dictionaryPosition].name = incomingName;
                stationArray[dictionaryPosition].port = incomingPort;
                dictionaryPosition++;
                */
                
                // First you have to run this: strtok(buf, ":");
                // This is their name: strtok(0, ":");
                // This is their port: strtok(0, ":");
            }
        }
    }
}   