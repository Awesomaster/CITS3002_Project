// Starting code for CITS3002 in C

// Headers
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <regex.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>

#define MAXDATASIZE 4096 // Max data size
#define MAXNAMESIZE 20 // Max size of station name
#define MAXTIMETABLE 50 // Max number of trips in a timetable
#define LISTENQ 4 // Max number of client connections

// Structure to place timetable in
struct Timetable {
    char destination[MAXNAMESIZE];
    char stop[MAXNAMESIZE];
    int leaveTime;
    int arrivalTime;
} timetable;

// Send a message to a given port UDP
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

    /* WAIT FOR RESPONSE
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

// Sends a signal to all adjacent ports with message
int broadcast(int *adjacentPorts, char *message, int numberOfAdjacentStations) {
    for (int i=0; i<numberOfAdjacentStations; i++) { 
        if(adjacentPorts[i] != 0) {
            udpSend(adjacentPorts[i], message);
        }
    }
    return 0;
}

// Returns the max of two ints x and y
int max(int x, int y) {
    if (x>y) return x;
    else return y;
}

// Returns current time in format HHMM as an integer
int getTime() {
    int hours, minutes;
    time_t currentTime = time(NULL);
    time(&currentTime); 
    struct tm *localTime = localtime(&currentTime);
    hours = localTime->tm_hour;
    minutes = localTime->tm_min;
    return hours*100+minutes;
}

int main(int argc, char **argv) {
    printf("Code is starting at %i\n", getTime());
    ;
    // Variables defined from argv
    char *name = argv[1]; // the spoken name of the station that will be used to refer to the station
    int tcpPort = atoi(argv[2]); // port for tcp connection from e.g. http://localhost:port
    int udpPort = atoi(argv[3]); // port for udp for other stations to use to communicate with this station 
    int numberOfAdjacentPorts = argc-4;

    int adjacentPorts[numberOfAdjacentPorts];
    for (int i=0; i<(numberOfAdjacentPorts); i++) {
        adjacentPorts[i] = atoi(argv[i+4]);
    }
    
    // Initialising Variables for station name-port dictionary
    char stationNameArray[argc-4][MAXNAMESIZE];
    bzero(stationNameArray, sizeof(stationNameArray));
    int stationPortArray[argc-4];
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

    // Printing and Initialising Timetable
    FILE *fp;
    char timetableFileName[MAXNAMESIZE+5];
    char line[100];
    struct Timetable timetable[MAXTIMETABLE];
    memset(timetable, 0, sizeof(timetable));
    int timetablePos = -1;
    strcpy(timetableFileName, "tt-");
    strcat(timetableFileName, name);

    fp = fopen(timetableFileName, "r");
    while(fgets(line, sizeof(line), fp) != NULL) {
        fputs(line, stdout);

        if (timetablePos >= 0) {
            char *leaveTimeStr = strtok(line, ",");
            char *stop = strtok(0, ",");
            char *arrivalTimeStr = strtok(0, ",");
            char *destination = strtok(0, ",");
            destination[strlen(destination)-1]='\0';

            int leaveTime = atoi(strtok(leaveTimeStr, ":"))*100 + atoi(strtok(0, ":"));
            int arrivalTime = atoi(strtok(arrivalTimeStr, ":"))*100 + atoi(strtok(0, ":"));
            
            timetable[timetablePos].leaveTime = leaveTime;
            timetable[timetablePos].arrivalTime = arrivalTime;
            strcpy(timetable[timetablePos].stop, stop);
            strcpy(timetable[timetablePos].destination, destination);
        }
        timetablePos++;
        // Here we store the timetable in some better way (preferably a struct made for it, we also need to properly interperit the time in a way that is consistent across the codes)
    }
    fclose(fp);

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

    // This is to find the name of all adjacent stations
    char broadcastMessage[50];
    sprintf(broadcastMessage, "NAME:%s:%s", name, argv[3]);
    broadcast(adjacentPorts, broadcastMessage, numberOfAdjacentPorts);

    // Initialisations for select
    FD_ZERO(&rset);
    maxfd = max(listenfd, udpfd) + 1;

    // Both TCP and UDP Servers set up
    // Listen to UDP and TCP
    printf("%s %s\n", name, "is running... waiting for connections...");
    printf("Listening on ports\n");
    for(;;) {
        FD_SET(listenfd, &rset);
        FD_SET(udpfd, &rset);
        nready = select(maxfd, &rset, NULL, NULL, NULL);
        
        // Incoming TCP Connection
        if (FD_ISSET(listenfd, &rset)) {
            clientLen = sizeof(clientAddress);
            connfd = accept(listenfd, (struct sockaddr *) &clientAddress, &clientAddress);
            printf("%s\n", "Received request...");
            if((childpid = fork()) == 0) {
                printf("%s\n", "Child created for dealing wih client requests");
                close(listenfd);

                bzero(buf, sizeof(buf));
                recv(connfd, buf, sizeof(buf),0);
                printf("%s: ", "String recieved from TCP");
                //puts(buf);

                printf("%s\n", "Adjacent Stations:");
                for (int i =0; i<(dictionaryPosition); i++) {
                    printf("%s is on port %i\n", stationNameArray[i], stationPortArray[i]);
                }

                strtok(buf, "="); // Get rid of everything before the =
                char *destinationStation = strtok(0, " "); // Get rid of everything after the destination name
                printf("TOOT TOOT, DESTINATION: %s\n", destinationStation);
                
                char time[4];
                int timeInt = getTime();
                sprintf(time, "%d", timeInt);

                char finalResult[MAXDATASIZE]; // This will be the time of arrival and path to destination

                // This might be able to be done in a seperate function and probably should because we will need to send the intial request there but this is similar to the code that wwill need to happpen if a particular stop doesnt have the destination station adjacent
                // is destinationStation in stationNameArray: IF YES = find next trip there, IF NO = broadcast
                int isAdjacent = 0;
                for (int i =0; i<dictionaryPosition; i++) {
                    if(strcmp(stationNameArray[i], destinationStation) == 0) {
                        isAdjacent = 1;
                        for (int j = 0; j<timetablePos; j++) {
                            if (strcmp(timetable[j].destination, destinationStation) == 0) {
                                if (timetable[j].leaveTime >= timeInt) {
                                    sprintf(finalResult, "You should be able to hop on at stop: %s at %i, arriving at station: %s at %i", timetable[j].stop, timetable[j].leaveTime, timetable[j].destination, timetable[j].arrivalTime);
                                    break;
                                }
                            }
                        }
                        // Look through timetable, from the time to find the next avaliable trip to that destination
                    }
                }

                if (isAdjacent == 0) {
                    // Here we become a UDP client after we find the port of the station we are trying to get to
                    printf("%s\n", "Not adjacent");
                    char request[MAXDATASIZE];
                    bzero(request, sizeof(request));
                    
                    int timeArrivingAtPort = 0;
                    for (int i =0; i<sizeof(adjacentPorts)/sizeof(int); i++) { 
                        for (int j = 0; j<timetablePos; j++) {
                            if (strcmp(timetable[j].destination, stationNameArray[i]) == 0) {
                                if (timetable[j].leaveTime >= timeInt) {
                                    sprintf(request, "PATH:%s:%i:%s-%s-%i", destinationStation, timetable[j].arrivalTime, name, time, timetable[j].arrivalTime);
                                    printf("Request sent: %s\n", request);
                                    break;
                                }
                            }
                        }
                        udpSend(stationPortArray[i], request);
                    }
                    // We dont use broadcast(adjacentPorts, request); since we have special conditions for each station
                }

                // Reply to TCP connection
                send(connfd, finalResult, strlen(finalResult), 0); // Send time of arrival and path of destination back to the web
                printf("Replied to TCP Connection. TCP Over.\n");
                
                close(connfd);
                exit(0);
            }
            close(connfd);
        }

        // Incoming UDP Message
        if (FD_ISSET(udpfd, &rset)) {
            printf("%s", "String recieved from UDP: ");
            bzero(buf, sizeof(buf));
            recvfrom(udpfd, buf, MAXDATASIZE, 0, (struct sockaddr*) &clientAddress, sizeof(clientAddress));
            puts(buf);

            // ------------------ WIP ------------------
            // Check the request (using regex)
            regexCheck = regcomp(&regex, "PATH:", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); // This will check if "PATH" as a string exists within the UDP message, meaning that they want to know how to get somewhere
            if (regexCheck == 0) { // They are asking for the path to their destination
                printf("Regex Check Success: PATH\n");
                strtok(buf, ":"); // This is to remove PATH
                char *destinationStation = strtok(0, ":");
                char *arriveHere = strtok(0, ":"); // Time it arrives at this station
                char *otherSteps = strtok(0, ":"); // This is the first step in the journey, we can use this to tell if we are finished and other things

                char messageToPassForward[MAXDATASIZE];
                for (int i =0; i<sizeof(adjacentPorts)/sizeof(int); i++) { 
                    for (int j = 0; j<timetablePos; j++) {
                        if (strcmp(timetable[j].destination, stationNameArray[i]) == 0) { // We want to be able to remove the stations we have already been to on this journey from our stationNameArray
                            if (timetable[j].leaveTime >= arriveHere) {
                                printf("Request passing forward: %s\n", messageToPassForward);
                                sprintf(messageToPassForward, "PATH:%s:%i:%s, %s-%s-%i", destinationStation, timetable[j].arrivalTime, otherSteps, name, arriveHere, timetable[j].leaveTime);
                                break;
                            }
                        }
                    }
                    udpSend(stationPortArray[i], messageToPassForward);
                }

                char *reply = "idk yet";
                int port = 1; // this will be set to the guy it needs to be
                //reply = findBestPath(broadcast(adjacentPorts, task));
                //udpSend(port, reply);

                /*
                int isAdjacent = 0;
                for (int i =0; i<(dictionaryPosition); i++) {
                    if(strcmp(stationNameArray[i], destinationStation)==0) {
                        printf("oh this should be easy\n");
                        isAdjacent = 1;
                    }
                }

                if (isAdjacent == 0) {
                    // oh boy
                    broadcast()
                }
                */

                sendto(connfd, reply, strlen(reply), 0, (struct sockaddr*) &clientAddress, sizeof(clientAddress)); 
            } 

            regexCheck = regcomp(&regex, "NAME", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); // This will check if "NAME" as a string exists within the UDP message, meaning that they want to know our name 
            if (regexCheck == 0) { // They are asking for our name
                printf("Regex Check Success: NAME\n");
                strtok(buf,":"); // This is required to get to the next tokens 

                // We can add this to our dictionary, meaning by the time all the stations have been created and performed the broadcast, every station will have a complete dictionary of adjacent stations names and ports
                char returnName[20];
                strcpy(returnName, strtok(0,":"));
                int returnPort = atoi(strtok(0,":"));
                
                // Constructing reply string
                char sendingName[50];
                strcpy(sendingName, "IAM:");
                strcat(sendingName, name);
                strcat(sendingName, ":");
                strcat(sendingName, argv[3]);

                bzero(stationNameArray[dictionaryPosition], sizeof(stationNameArray[dictionaryPosition]));
                strcpy(stationNameArray[dictionaryPosition], returnName);
                stationPortArray[dictionaryPosition] = returnPort;
                dictionaryPosition++;

                udpSend(returnPort, sendingName);
                printf("Replied to NAME request with: %s\n", sendingName);
            } 

            regexCheck = regcomp(&regex, "IAM:", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); // This will be a reply from a name request
            if (regexCheck == 0) { // This is a reply meaning that they have given us their name
                printf("Regex Check Success: IAM\n");
                
                // Add station to station dictionary
                char incomingName[20];
                strtok(buf, ":");
                strcpy(incomingName, strtok(0,":"));
                int incomingPort = atoi(strtok(0,":"));
                bzero(stationNameArray[dictionaryPosition], sizeof(stationNameArray[dictionaryPosition]));
                strcpy(stationNameArray[dictionaryPosition], incomingName);
                stationPortArray[dictionaryPosition] = incomingPort;
                dictionaryPosition++;
            }
            close(connfd);
        }
    }
}   