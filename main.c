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

#define MAXDATASIZE 1024 // Max data size
#define MAXNAMESIZE 20 // Max size of station name
#define MAXTIMETABLE 50 // Max number of trips in a timetable
#define LISTENQ 4

// Structure to place timetable in
struct Timetable {
    char destination[MAXNAMESIZE];
    char stop[MAXNAMESIZE];
    char vehicle[MAXNAMESIZE];
    int leaveTime;
    int arrivalTime;
} timetable;

// Send a message to a given port UDP
void udpSend(int stationPort, char *message) {
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

    close(udpOutfd);
}

// Sends a signal to all adjacent ports with message
void broadcast(int *adjacentPorts, char *message, int numberOfAdjacentStations) {
    for (int i=0; i<numberOfAdjacentStations; i++) { 
        if(adjacentPorts[i] != 0) {
            udpSend(adjacentPorts[i], message);
        }
    }
}

// Returns the max of two ints x and y
int max(int x, int y) {
    if (x>y) return x;
    else return y;
}

int timeDif(int timeA, int timeB) {
    int hourDif = timeA/100 - timeB/100;
    int minuteDif = timeA%100 - timeB%100;
    return hourDif*60 + minuteDif;
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
    int tcpPort = atoi(argv[2]); // port for tcp connection from e.g. http://localhost:tcpport
    int udpPort = atoi(argv[3]); // port for udp for other stations to use to communicate with this station 
    
    // every other one after our UDP port is the UDP port of an adjacent station
    int numberOfAdjacentPorts = argc-4; 
    int adjacentPorts[numberOfAdjacentPorts];
    for (int i=0; i<(numberOfAdjacentPorts); i++) {
        adjacentPorts[i] = atoi(argv[i+4]);
    }
    
    // Initialising Variables for station name-port dictionary (just as two arrays)
    char stationNameArray[argc-4][MAXNAMESIZE]; //bzero(stationNameArray, sizeof(stationNameArray));
    int stationPortArray[argc-4];
    int dictionaryPosition = 0;

    // Initialising Variables to look back to for return message history
    int numberOfPathsSent = 0;
    int numberYouWillRecieveFrom = 0;
    int portsToBeRecievedFrom[numberOfAdjacentPorts];
    int numberOfRepliesRecived = 0;
    char repliesRecieved[numberOfAdjacentPorts][MAXDATASIZE];

    // Initialising shortest path time 
    int shortestPathTime = 24*60; // A whole day, meaning the first path it recieves will be smaller and will set it to the smallest

    // Final Result that will be returned to the web
    char finalResult[MAXDATASIZE]; // This will be the time of arrival and path to destination

    // Variabled required for socket connections
    int tcpFD, udpFD, maxfd, connFD, externalfd, n, nready;
    fd_set rset;
    pid_t childpid;
    socklen_t clientLen;
    char buf[MAXDATASIZE];
    struct sockaddr_in clientAddress, tcpServerAddress, udpServerAddress, externalUdpServerAddress;

    // Regex variables
    regex_t regex;
    int regexCheck = 0;

    // Variables used for responses
    char *headerResponse = "HTTP/1.1 200 OK\nContent-Type: text/html\nConnection: Closed\n\n";
    char *htmlHeader = "<html>\n<body>";
    char *goBack =  "</body>\n</html>\n"; // for goback button <a href=\"file:///home/josh/Desktop/CITS3002/myform.html\">Go Back</a>\n

    // Printing and Initialising Timetable
    FILE *fp;
    char timetableFileName[MAXNAMESIZE+5];
    char line[100];
    struct Timetable timetable[MAXTIMETABLE];
    memset(timetable, 0, sizeof(timetable));
    int timetablePos = -1; // this needs to be -1 so that it doesnt read the first line of the file as a timetable slot
    strcpy(timetableFileName, "tt-");
    strcat(timetableFileName, name);

    // NOTE: Here we need to implement a checksum feature that we look back on every once in a while to make sure that the timetable file hasnt changed, and if it has we need to update it
    fp = fopen(timetableFileName, "r");
    while(fgets(line, sizeof(line), fp) != NULL) {
        fputs(line, stdout);
        // To skip the first line
        if (timetablePos >= 0) {
            char *leaveTimeStr = strtok(line, ",");
            char *vehicle = strtok(0, ",");
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
            strcpy(timetable[timetablePos].vehicle, vehicle);
        }
        timetablePos++;
    }
    fclose(fp);

    // TCP Setup
    tcpFD = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&tcpServerAddress, sizeof(tcpServerAddress));
    tcpServerAddress.sin_family = AF_INET;
    tcpServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    tcpServerAddress.sin_port = htons(tcpPort); 

    // Binding and listening with TCP
    bind(tcpFD, (struct sockaddr *) &tcpServerAddress, sizeof(tcpServerAddress));
    listen(tcpFD, LISTENQ);
    
    // UDP Setup
    udpFD = socket(AF_INET, SOCK_DGRAM, 0);
    udpServerAddress.sin_family = AF_INET;
    udpServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    udpServerAddress.sin_port = htons(udpPort);
    
    // Binding UDP
    bind(udpFD, (struct sockaddr *) &udpServerAddress, sizeof(udpServerAddress));

    // This is to find the name of all adjacent stations, and to send to all adjacent stations so they know who this station is
    char broadcastMessage[50];
    snprintf(broadcastMessage, 50*sizeof(char), "NAME:%s:%s", name, argv[3]);
    broadcast(adjacentPorts, broadcastMessage, numberOfAdjacentPorts);

    // Initialisations for select
    FD_ZERO(&rset);
    maxfd = max(tcpFD, udpFD) + 1;

    // Both TCP and UDP Servers set up
    // Listen to UDP and TCP
    printf("%s %s\n", name, "is running... waiting for connections...");
    printf("Listening on ports TCP: %i, UDP: %i\n", tcpPort, udpPort);

    int haveWeAccepted = 0;
    while(1) {
        FD_SET(tcpFD, &rset);
        FD_SET(udpFD, &rset);
        nready = select(maxfd, &rset, NULL, NULL, NULL);
        
        // Incoming TCP Connection
        if (FD_ISSET(tcpFD, &rset) && haveWeAccepted == 0) {
            haveWeAccepted = 1;
            clientLen = sizeof(clientAddress);
            
            connFD = accept(tcpFD, (struct sockaddr *) &clientAddress, &clientAddress);
            printf("%s\n", "Received request...");
            printf("%s\n", "Dealing wih client requests");
            close(tcpFD);

            bzero(buf, sizeof(buf));
            recv(connFD, buf, sizeof(buf),0); 
            printf("%s\n", "String recieved from TCP");

            // Checking that it is actually a journey request
            char *bufStart = strdup(buf);
            bufStart[8] = '\0';
            printf("%s\n", bufStart); // Prints the first 8 characters
            if (strcmp(bufStart,"GET /?to") == 0) {
                strtok(buf, "="); // Get rid of everything before the =
                char *destinationStation = strtok(0, " "); // Get destination name
                printf("Destination: %s\n", destinationStation);
                
                char time[4];
                int timeInt = getTime();

                // FOR TESTING WE SET TIME EARLY TO ACTUALLY GET PATHS BECAUSE IM CODING TOO LATE

                // Get time as a char* for when we need it as a str
                snprintf(time, 4*sizeof(char), "%d", timeInt);

                int isAdjacent = 0; // is destinationStation in stationNameArray (i.e. is it adjacent): IF YES = find next trip there, IF NO = broadcast
                int isAvaliable = 0; // Used to track if there is a trip or not given that it is adjacent

                // Handles it if we are adjacent
                printf("lets check if we are adjacent\n");
                for (int i =0; i<dictionaryPosition; i++) {
                    printf("stationNameArray[i]: %s, destination: %s\n", stationNameArray[i], destinationStation);
                    if(strcmp(stationNameArray[i], destinationStation) == 0) {
                        printf("yayy we are adjacent\n");
                        isAdjacent = 1;
                        for (int j = 0; j<timetablePos; j++) {
                            if (strcmp(timetable[j].destination, destinationStation) == 0) {
                                printf("cur time: %i, tt time: %i\n", timeInt, timetable[j].leaveTime);
                                if (timetable[j].leaveTime >= timeInt) {
                                    isAvaliable = 1;
                                    int leaveTimeHours = timetable[j].leaveTime/100;
                                    int leaveTimeMins = timetable[j].leaveTime%100;
                                    int arriveTimeHours = timetable[j].arrivalTime/100;
                                    int arriveTimeMinutes = timetable[j].arrivalTime%100;
                                    char leaveHrType[3];
                                    char arriveHrType[3];
                                    if (leaveTimeHours >= 12) {
                                        sprintf(leaveHrType, "PM");
                                        if (leaveTimeHours > 12) {
                                            leaveTimeHours-=12;
                                        }
                                    } else {
                                        sprintf(leaveHrType, "AM");
                                    }
                                    if (arriveTimeHours >= 12) {
                                        sprintf(arriveHrType, "PM");
                                        if (arriveTimeHours > 12) {
                                            arriveTimeHours-=12;
                                        }
                                    } else {
                                        sprintf(arriveHrType, "AM");
                                    }
                                    // Add total trip time
                                    snprintf(finalResult, MAXDATASIZE*sizeof(char), "From: %s hop on %s at %s<br>To: %s<br>Leaving at: %i past %i%s<br>Arriving at: %i past %i%s", name, timetable[j].vehicle, timetable[j].stop, timetable[j].destination, leaveTimeMins, leaveTimeHours, leaveHrType, arriveTimeMinutes, arriveTimeHours, arriveHrType);
                                    break;
                                }
                            }
                        }
                        if (isAvaliable == 0) {
                            sprintf(finalResult, "Its too late for a trip today, try again earlier tomorrow ;(");
                        }
                        
                        // REPLYING TO TCP WITH EITHER SUCCESS OR FAILURE BASED ON ADJACENCY
                        char returnToTCP[MAXDATASIZE];
                        snprintf(returnToTCP, MAXDATASIZE*sizeof(char)*2, "%s%s<h2>%s</h2>%s", headerResponse, htmlHeader, finalResult, goBack);
                        send(connFD, returnToTCP, strlen(returnToTCP), 0); // Send time of arrival and path of destination back to the web
                        printf("Returned: %s\n", returnToTCP);
                        printf("Replied to TCP Connection. TCP Over.\n");
                        haveWeAccepted = 0;
                        close(connFD);
                    }
                }

                // Handles it if we are not adjacent
                if (isAdjacent == 0) {
                    printf("%s %s %s %s\n", "We", name,  "are not adjacent to the destination:", destinationStation);
                    char request[MAXDATASIZE];
                    bzero(request, sizeof(request)); // Here we make the request empty, probably not necessary

                    int pathAvaliable = 0;
                    for (int i = 0; i < numberOfAdjacentPorts; i++) { // We are going through all the adjacent ports
                        printf("i: %i, station: %s\n", i, stationNameArray[i]);
                        for (int j = 0; j < timetablePos; j++) { // Checking each timetable event for any that go to the current station we are looking at
                            printf("cur timetable dest: %s\n", timetable[j].destination);
                            if (strcmp(timetable[j].destination, stationNameArray[i]) == 0) { // We have found a timetable event that goes to this station
                                printf("cur time: %i, tt time: %i\n", timeInt, timetable[j].leaveTime);
                                if (timetable[j].leaveTime >= timeInt) { // Since timetable listings are in time order, we just need to find the first one that leaves after our current time
                                    pathAvaliable = 1;
                                    int onTransportTime = timetable[j].arrivalTime - timetable[j].leaveTime; // If we take the time we leave this station
                                    snprintf(request, MAXDATASIZE*sizeof(char), "PATH:%s:%i:%i:%s-%i-%i-%s-%s", destinationStation, timetable[j].arrivalTime, onTransportTime, name, onTransportTime, timetable[j].leaveTime, timetable[j].stop, timetable[j].vehicle);
                                    // FORMAT OF PATH REQUEST -> PATH:destination:arriveAtDestination:timeOnTransport:thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation-vehicleType
                                    printf("Request: %s, sent to: %s (port: %i)\n", request, stationNameArray[i], stationPortArray[i]);
                                    break;
                                }
                            }
                        }
                        if (pathAvaliable == 1) { // we are using this to tell if we did actually get an avaliable time
                            udpSend(stationPortArray[i], request); // Actually sends the request
                            int dontNeedRecieve = 0;
                            for (int j = 0; j < numberYouWillRecieveFrom; j++) {
                                if (stationPortArray[i] == portsToBeRecievedFrom[j]) {
                                    dontNeedRecieve = 1;
                                }
                            }
                            if (dontNeedRecieve == 0) {
                                portsToBeRecievedFrom[numberYouWillRecieveFrom] = stationPortArray[i];
                                numberYouWillRecieveFrom += 1;
                            }
                            numberOfPathsSent += 1;
                            pathAvaliable = 0;
                        }
                        
                    }

                    if (numberOfPathsSent == 0) { // This is the case that we are not adjacent to the station we need to get to, and none of our adjacent stations have timetable slots avaliable
                        snprintf(finalResult, MAXDATASIZE*sizeof(char), "Its too late for a trip today, try again earlier tomorrow ;(");    
                        char returnToTCP[MAXDATASIZE];
                        snprintf(returnToTCP, 2*MAXDATASIZE*sizeof(char), "%s%s<h2>%s</h2>%s", headerResponse, htmlHeader, finalResult, goBack);
                        send(connFD, returnToTCP, strlen(returnToTCP), 0); // Send time of arrival and path of destination back to the web
                        printf("Returned: %s\n", returnToTCP);
                        printf("Replied to TCP Connection. TCP Over.\n");
                        haveWeAccepted = 0;
                        close(connFD);
                    }
                }
            } else { // In the event that we dont get a ?to request
                haveWeAccepted = 0;
                close(connFD);
            }

            bzero(bufStart, sizeof(bufStart));
        }

        // Incoming UDP Message
        if (FD_ISSET(udpFD, &rset)) {
            printf("%s", "String recieved from UDP: "); // We know we got a UDP message
            bzero(buf, sizeof(buf)); // Make sure the buffer is empty first
            recvfrom(udpFD, buf, MAXDATASIZE*sizeof(char), 0, (struct sockaddr*) &clientAddress, sizeof(clientAddress)); // Recieves from the client
            puts(buf); // Outputs the data we recieved from the client, this should be q request which is sorted by the regexCheck

            // THIS IS A PATH REQUEST RECIEVED
            regexCheck = regcomp(&regex, "PATH:", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); 
            if (regexCheck == 0) { // They are asking for the path to their destination
                printf("Regex Check Success: PATH -> %s\n", buf);
                strtok(buf, ":"); // This is to remove PATH

                // FORMAT OF PATH REQUEST -> PATH:destination:arriveAtDestination:timeOnTransport:history,thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation
                char *destinationStation = strtok(0, ":"); // Where we want to get
                int arriveHere = atoi(strtok(0, ":")); // Time it arrives at this station
                int totalTransportTime = atoi(strtok(0,":")); // Time travelled on transport so far
                char *otherSteps = strtok(0, ":"); // This is the first step in the journey, we can use this to tell if we are finished and other things
                char *otherStepsForReply = strdup(otherSteps); // Duplicate used because using otherSteps again is kind of buggy

                int hasArrived = 0; // This will be used to tell if we have arrived at the destination and will change the type of reply

                char arrayOfVisited[numberOfAdjacentPorts][MAXNAMESIZE]; // An array of stations that are in the history log that is being passed around between the stations
                char *stationVisited;
                stationVisited = strtok(otherSteps, "-"); // This will be the first stop visited based on the path structure (otherSteps is the history and the first part in each history section is the station name)
                int numberOfStationsVisited = 0; //

                while (stationVisited != NULL) {
                    snprintf(arrayOfVisited[numberOfStationsVisited], MAXNAMESIZE*sizeof(char), "%s", stationVisited); // Since we know it isnt null we can copy it into our array
                    strtok(0,","); // Getting to the next step of the history (each step is seperated by a comma)
                    stationVisited = strtok(0, "-"); // Reassigning the stationVisited
                    numberOfStationsVisited++; // We have added one more station
                }

                char messageToPassForward[MAXDATASIZE]; // This is the message that we will sending in either our PATH or RET message depending on if we are adjacent to the destination
                
                char toBeVisitedStationNameArray[numberOfAdjacentPorts][MAXNAMESIZE]; // This is the array of stations names that are adjacent to us, but are not in this messages history
                int toBeVisitedStationPortArray[numberOfAdjacentPorts]; // This is the array of stations ports that are adjacent to us, but are not in this messages history

                int toBeVisitedArraySize = 0;
                int badEgg = 0;
                for (int i = 0; i < numberOfAdjacentPorts; i++) { // Go through all adjacent ports
                    for (int j = 0; j < numberOfStationsVisited; j++) { // Go through all visited ports
                        if (strcmp(arrayOfVisited[j], stationNameArray[i]) == 0) { // If it is adjacent visited then we dont add it to our array
                            badEgg = 1; // this one is has already been visited
                            printf("%s: HERE WE HAVE THE ONE THAT IS THE SAME FOR i=%i, STATION: %s\n", name, i, stationNameArray[i]);
                        }
                    }
                    if (badEgg == 0) { // this one has not been visited
                        printf("%s: We are adding %s to be visited\n", name, stationNameArray[i]);
                        snprintf(toBeVisitedStationNameArray[toBeVisitedArraySize], MAXNAMESIZE*sizeof(char), "%s", stationNameArray[i]);
                        toBeVisitedStationPortArray[toBeVisitedArraySize] = stationPortArray[i];
                        toBeVisitedArraySize++;
                    } else {
                        badEgg = 0;
                    }
                }

                int amISendingPath = 0;
                printf("%s: Stations Visitied: %i, Adjacent Ports: %i, Scuffed Array: %i\n", name, numberOfStationsVisited, numberOfAdjacentPorts, toBeVisitedArraySize);
                if (toBeVisitedArraySize > 0) {
                    for (int i = 0; i < toBeVisitedArraySize; i++) { // We go through all of the adjacent stations that havent seen this message yet
                        for (int j = 0; j<timetablePos; j++) {
                            if (strcmp(timetable[j].destination, toBeVisitedStationNameArray[i]) == 0) { // We want to be able to remove the stations we have already been to on this journey from our stationNameArray
                                if (timetable[j].leaveTime >= arriveHere) {
                                    // We need to calculate time taken like this because we are sorting the number as a 4 digit number, meaning that if a train leaves at 1250 and arrives at 1303, timeTaken will be 53 minutes instead of 13, which this below will fix
                                    int timeTaken = timeDif(timetable[j].arrivalTime, timetable[j].leaveTime);
                                    if (strcmp(timetable[j].destination, destinationStation) == 0) {
                                        // SRET means that it is returning the path and it was sucessful
                                        snprintf(messageToPassForward, MAXDATASIZE*sizeof(char), "SRET:%i:%s:%i:%i:%s,%s-%i-%i-%s-%s", numberOfStationsVisited-1, destinationStation, timetable[j].arrivalTime, totalTransportTime+timeTaken, otherStepsForReply, name, timeTaken, timetable[j].leaveTime, timetable[j].stop, timetable[j].vehicle);
                                        printf("%s: Adjacent to destination. Sending: %s back to: %s (port: %i) for checking!!!\n", name, messageToPassForward, stationNameArray[i], stationPortArray[i]);
                                        hasArrived = 1;
                                        break;
                                    } else {
                                        printf("%s: stationThatIsntDestination: %s\n", name, toBeVisitedStationNameArray[i]);
                                        snprintf(messageToPassForward, MAXDATASIZE*sizeof(char), "PATH:%s:%i:%i:%s,%s-%i-%i-%s-%s", destinationStation, timetable[j].arrivalTime, totalTransportTime+timeTaken, otherStepsForReply, name, timeTaken, timetable[j].leaveTime, timetable[j].stop, timetable[j].vehicle);
                                        // FORMAT OF PATH REQUEST -> PATH:destination:arriveAtDestination:timeOnTransport:history,thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation-vehicleType
                                        printf("%s: Request: %s, passed forward to: %s (port: %i)\n", name, messageToPassForward, toBeVisitedStationNameArray[i], toBeVisitedStationPortArray[i]);
                                        hasArrived = 0;
                                        amISendingPath = 1;
                                        numberOfPathsSent += 1;
                                        int dontNeedRecieve = 0;
                                        for (int k = 0; k < numberYouWillRecieveFrom; k++) {
                                            if (toBeVisitedStationPortArray[i] == portsToBeRecievedFrom[k]) {
                                                dontNeedRecieve = 1;
                                            }
                                        }
                                        if (dontNeedRecieve == 0) {
                                            portsToBeRecievedFrom[numberYouWillRecieveFrom] = toBeVisitedStationPortArray[i];
                                            numberYouWillRecieveFrom += 1;
                                        }
                                        break;
                                    }
                                } else { // This is the case that we are adjacent to the destination, but it is too late for a trip and thus we have a failed path, this we can deal with by either returning it as a failure or ignore it
                                    if (strcmp(toBeVisitedStationNameArray[i], destinationStation) == 0) {
                                        if ((strcmp(timetable[j].destination, destinationStation) == 0) && (hasArrived == 0)) {
                                            snprintf(messageToPassForward, MAXDATASIZE*sizeof(char), "FRET:%i:%s:%i:%i:%s,%s-%i-%i-%s-%s", numberOfStationsVisited-1, destinationStation, 0, 24*60, otherStepsForReply, name, 0, 0, "NA", "NA");
                                            printf("%s: Adjacent to destination, but there are no journeys. Sending: %s back to: %s (port: %i) for checking!!!\n", name, messageToPassForward, stationNameArray[i], stationPortArray[i]);
                                            hasArrived = 1;
                                        }
                                    }
                                }
                            } 
                        }
                        printf("%s: hasArrived before send: %i\n", name, hasArrived);

                        if (hasArrived == 1) { // This is a reply
                            printf("%s: looks like we are sending back to person who sent to us %s\n", name, arrayOfVisited[numberOfStationsVisited-1]);
                            for (int k = 0; k < numberOfAdjacentPorts; k++) {
                                printf("%s: Surely one of these %s (an adjacent) is %s\n", name, stationNameArray[k], arrayOfVisited[numberOfStationsVisited-1]);
                                if (strcmp(arrayOfVisited[numberOfStationsVisited-1], stationNameArray[k]) == 0) {
                                    printf("%s: passin back the footy\n", name);
                                    udpSend(stationPortArray[k], messageToPassForward); // This will pass the message back to the previous station
                                    break;
                                }
                            }
                            hasArrived = 0;
                        } else if (amISendingPath == 1) { // This is a path request
                            printf("%s: I should be sending a path request right now, \nHere: %s\n", name, messageToPassForward);
                            udpSend(toBeVisitedStationPortArray[i], messageToPassForward);
                            amISendingPath = 0;
                        }
                        printf("%s: we have checked %s, now to check next adjacent station (if there is one)\n", name, toBeVisitedStationNameArray[i]);
                    }
                } else {                                            
                    // We will start returning at this point, since we cant go any further
                    for (int i = 0; i < numberOfAdjacentPorts; i++) {
                        printf("%s: we start FRET to %s, comparing to %s\n", name, arrayOfVisited[numberOfStationsVisited-1], stationNameArray[i]);
                        if (strcmp(stationNameArray[i],arrayOfVisited[numberOfStationsVisited-1]) == 0) {
                            printf("%s: successfully returned failed\n", name);
                            snprintf(messageToPassForward, MAXDATASIZE*sizeof(char), "FRET:%i:%s:%i:%i:%s,%s-%i-%i-%s-%s", numberOfStationsVisited-1, destinationStation, 0, 24*60, otherStepsForReply, name, 0, 0, "NA", "NA");
                            udpSend(stationPortArray[i], messageToPassForward);
                            break;
                        }                            
                    }
                }
                // } else { // This is the case where we didnt get a faster response, in this case we could return failed path, not sure yet

                // }
            }

            // THIS IS A RETURN REQUEST RECIEVED
            regexCheck = regcomp(&regex, "RET:", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); // This will check if its a return message, and then will sort it into successful and failed returns
            if (regexCheck == 0) {
                printf("%s: Regex Check Success: RET\n", name);
                
                int isSuccess = 0;
                regexCheck = regcomp(&regex, "SRET:", 0);
                regexCheck = regexec(&regex, buf, 0, NULL, 0); // This means the return was a success, and we will treat it accordingly
                if (regexCheck == 0) {
                    isSuccess = 1; // If this is not the case, then it is a failure (because we will get an FRET)
                    printf("%s: what a great sucess this is!!\n", name);
                }
                
                // We dont want to be counting replies that come to us twice that are the same
                int isThisOldReply = 0;
                for (int i; i < numberOfRepliesRecived; i++) {
                    if (strcmp(buf, repliesRecieved[i]) == 0) {
                        isThisOldReply = 1;
                    } 
                }
                
                if (isThisOldReply == 0) { // This is a new reply
                    bzero(repliesRecieved[numberOfRepliesRecived], MAXDATASIZE*sizeof(char));
                    snprintf(repliesRecieved[numberOfRepliesRecived], MAXDATASIZE*sizeof(char), "%s", buf);
                    numberOfRepliesRecived += 1;
                }

                printf("%s: I have recieved %i replies, and I expect %i replies\n", name, numberOfRepliesRecived, numberYouWillRecieveFrom);
                // If we get a successful reply, we are winning, if not, we arent, oh well, but if we have got a bunch of replies, from everyone, and still no success, we give up and tell everyone that we give up      
                if ((isSuccess == 1) || (numberOfRepliesRecived >= numberYouWillRecieveFrom)) {
                    printf("%s: we are in\n", name);fflush(stdout);
                    char messageToPassBack[MAXDATASIZE]; // Initialising it for passing back the message
                    strtok(buf, ":"); // to get rid of the RET
                    int numberOfStationsLeft = atoi(strtok(0,":")); // This is the amount of stations visited in the return path left from this station (just added here for convinence/efficiency)
                    char *destinationStation = strtok(0, ":");
                    int arriveHere = atoi(strtok(0, ":")); // Time it arrives at this station
                    int totalTransportTime = atoi(strtok(0,":")); // Total time on transport so far (which we are using for optimising path)
                    char *otherSteps = strtok(0, ":"); // This is the first step in the journey, we can use this to tell if we are finished and other things
                    char *otherStepsForReply = strdup(otherSteps);

                    printf("%s: Breaking down the return message, noLeft: %i, dest: %s, arrHere: %i, timeTot: %i, otherSteps: %s\n", name, numberOfStationsLeft, destinationStation, arriveHere, totalTransportTime, otherSteps);fflush(stdout);

                    char arrayOfVisited[numberOfAdjacentPorts][MAXNAMESIZE]; // Array needed to keep track of where we have been
                    char *stationVisited = strtok(otherSteps, "-");
                    int numberOfStationsVisited = 0;
                    printf("%s: endering\n", name);fflush(stdout);
                    while (stationVisited != NULL) {
                        snprintf(arrayOfVisited[numberOfStationsVisited], MAXNAMESIZE*sizeof(char), "%s", stationVisited);
                        strtok(0,","); //go to next station
                        stationVisited = strtok(0, "-"); // station is the first in each "history" section of the reply
                        numberOfStationsVisited++;
                    }

                    if (numberOfStationsLeft > 0) { // We arent at the back to source yet, but we will send all our replies
                        printf("%s: We arent there yet, getting there\n", name);fflush(stdout);
                        if (isSuccess == 1) {
                            snprintf(messageToPassBack, MAXDATASIZE*sizeof(char), "SRET:%i:%s:%i:%i:%s", numberOfStationsLeft-1, destinationStation, arriveHere, totalTransportTime, otherStepsForReply);
                        } else {
                            snprintf(messageToPassBack, MAXDATASIZE*sizeof(char), "FRET:%i:%s:%i:%i:%s", numberOfStationsVisited-1, destinationStation, 0, 24*60, otherStepsForReply); // this other steps will be the best other steps
                        }
                        for (int  j= 0; j < numberOfAdjacentPorts; j++) {
                            printf("%s: only %i left!! we replyin as long as we are adjacent to %s, are we: %s (checking %i/%i)\n", name, numberOfStationsLeft-1, arrayOfVisited[numberOfStationsLeft-1], stationNameArray[j], j, numberOfAdjacentPorts);fflush(stdout);
                            if (strcmp(arrayOfVisited[numberOfStationsLeft-1], stationNameArray[j]) == 0) {
                                udpSend(stationPortArray[j], messageToPassBack); // This will pass the message back to the previous station
                            }
                        }
                    } else {
                        printf("WE HAVE ARRIVED AT SOURCE!!!!: Home time!\n");fflush(stdout);
                        // We are the final stop, our message has come back, now we want to process it 

                        char returnedStep[100];// for each one
                        char returnedSteps[MAXDATASIZE/2]; // to concat to
                        bzero(returnedSteps, sizeof(returnedSteps));
                        char *otherStepsFinalCopy = strdup(otherStepsForReply);
                        char *currStation = strtok(otherStepsFinalCopy, "-");
                        printf("WE HAVE ARRIVED AT SOURCE!!!!: currStation: %s", currStation);fflush(stdout);
                        while (currStation != NULL) {
                            int currTimeTaken = atoi(strtok(0, "-"));
                            int currLeaveHere = atoi(strtok(0, "-"));
                            int leaveHereHours = currLeaveHere/100;
                            int leaveHereMinutes = currLeaveHere%100;
                            char hrType[3];
                            if (leaveHereHours >= 12) {
                                sprintf(hrType, "PM");
                                if (leaveHereHours > 12) {
                                    leaveHereHours-=12;
                                }
                            } else {
                                sprintf(hrType, "AM");
                            }
                            char *currStop = strtok(0, "-");
                            char *currVehicle = strtok(0, ",");
                            snprintf(returnedStep, 100*sizeof(char), "<p>From %s, catch the %s at %s. Departing %i past %i%s, will take %i minutes.<br></p>\n", currStation, currVehicle, currStop, leaveHereMinutes, leaveHereHours, hrType, currTimeTaken);
                            strcat(returnedSteps, returnedStep);
                            printf("WE HAVE ARRIVED AT SOURCE!!!!: all steps so far: %s\n", returnedSteps);fflush(stdout);
                            currStation = strtok(0, "-");
                        }

                        // is the reply a success
                        if (isSuccess == 1) {
                            // this needs to be expanded
                            int arriveHereHours = arriveHere/100;
                            int arriveHereMinutes = arriveHere%100; 
                            char hrType[3];
                            if (arriveHereHours >= 12) {
                                sprintf(hrType, "PM");
                                if (arriveHereHours > 12) {
                                    arriveHereHours-=12;
                                }
                            } else {
                                sprintf(hrType, "AM");
                            }
                            snprintf(finalResult, MAXDATASIZE*sizeof(char), "<h2>You will arrive at %s, at %i past %i%s<br>It took %i minutes (on transport) to get here<br></h2>%s", destinationStation, arriveHereMinutes, arriveHereHours, hrType, totalTransportTime, returnedSteps);
                        } else {
                            snprintf(finalResult, MAXDATASIZE*sizeof(char), "<h2>There is no avaliable path left tonight</h2>");
                        }

                        printf("Choo choo we are home via: %s\n", finalResult);

                        char returnToTCP[MAXDATASIZE];
                        snprintf(returnToTCP, MAXDATASIZE*sizeof(char), "%s%s%s%s", headerResponse, htmlHeader, finalResult, goBack);
                        send(connFD, returnToTCP, strlen(returnToTCP), 0); // Send time of arrival and path of destination back to the web
                        printf("Returned: %s\n", returnToTCP);
                        printf("Replied to TCP Connection. TCP Over.\n");
                        haveWeAccepted = 0;
                        close(connFD);
                    }
                }
            }
            
            // THIS IS A NAME REQUEST RECIEVED, this means an adjacent station has started up and is giving us its name-port relation, we should reply with ours! (its just polite)
            regexCheck = regcomp(&regex, "NAME:", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); // This will check if "NAME" as a string exists within the UDP message, meaning that they want to know our name 
            if (regexCheck == 0) { // They are asking for our name
                printf("Regex Check Success: NAME\n");
                strtok(buf,":"); // This is required to get to the next tokens 

                // We can add this to our dictionary, meaning by the time all the stations have been created and performed the broadcast, every station will have a complete dictionary of adjacent stations names and ports
                char returnName[20];
                strcpy(returnName, strtok(0,":"));
                int returnPort = atoi(strtok(0,":"));
                
                int stationKnown = 0;
                for (int i = 0; i < numberOfAdjacentPorts; i++) {
                    if (returnPort == stationPortArray[i]) {
                        stationKnown = 1;
                    }
                }

                if (stationKnown == 0) {
                    // Constructing reply string
                    char sendingName[50];
                    snprintf(sendingName, 50*sizeof(char), "IAM:%s:%i", name, udpPort);
                
                    snprintf(stationNameArray[dictionaryPosition], MAXNAMESIZE*sizeof(char), "%s", returnName);
                    stationPortArray[dictionaryPosition] = returnPort;
                    dictionaryPosition++;
                    printf("I am %s, and I just added %s to my adjacency array\n", name, returnName);

                    udpSend(returnPort, sendingName);
                    printf("Replied to NAME request with: %s\n", sendingName);
                }
                
            }

            // THIS IS A IAM REQUEST RECIEVED (A REPLY)
            regexCheck = regcomp(&regex, "IAM:", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); // This will be a reply from a name request
            if (regexCheck == 0) { // This is a reply meaning that they have given us their name
                printf("Regex Check Success: IAM\n");

                char incomingName[20];
                strtok(buf, ":");
                strcpy(incomingName, strtok(0,":"));
                int incomingPort = atoi(strtok(0,":"));
                
                // Add station to station dictionary
                int stationKnown = 0;
                for (int i = 0; i < numberOfAdjacentPorts; i++) {
                    if (incomingPort == stationPortArray[i]) {
                        stationKnown = 1;
                    }
                }

                if (stationKnown == 0) {
                    snprintf(stationNameArray[dictionaryPosition], MAXNAMESIZE*sizeof(char), "%s", incomingName);
                    stationPortArray[dictionaryPosition] = incomingPort;
                    dictionaryPosition++;
                    printf("I am %s, and I just added %s to my adjacency array\n", name, incomingName);
                }
                
                
            }
            close(connFD);
        }
    }
}   