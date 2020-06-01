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

#define MAXDATASIZE 512 // Max data size
#define MAXNAMESIZE 20 // Max size of station name
#define MAXTIMETABLE 50 // Max number of trips in a timetable
#define LISTENQ 4

// Structure to place timetable in
struct Timetable {
    char destination[MAXNAMESIZE];
    char stop[MAXNAMESIZE];
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

    // Initialising shortest path variables 
    int shortestPathTime = 24*60; // A whole day, meaning the first path it recieves will be smaller and will set it to the smallest
    char shortestPath[MAXDATASIZE];

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
    char *htmlHeader = "<html>\n<body>\n<h2>";
    char *htmlEnder = "</h2>\n";
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
                // timeInt = 700;

                // Get time as a char* for when we need it as a str
                snprintf(time, 4*sizeof(char), "%d", timeInt);

                int isAdjacent = 0; // is destinationStation in stationNameArray (i.e. is it adjacent): IF YES = find next trip there, IF NO = broadcast
                int isAvaliable = 0; // Used to track if there is a trip or not given that it is adjacent

                // Handles it if we are adjacent
                for (int i =0; i<dictionaryPosition; i++) {
                    if(strcmp(stationNameArray[i], destinationStation) == 0) {
                        isAdjacent = 1;
                        for (int j = 0; j<timetablePos; j++) {
                            if (strcmp(timetable[j].destination, destinationStation) == 0) {
                                printf("cur time: %i, tt time: %i\n", timeInt, timetable[j].leaveTime);
                                if (timetable[j].leaveTime >= timeInt) {
                                    isAvaliable = 1;
                                    snprintf(finalResult, MAXDATASIZE*sizeof(char), "From: %s at stop: %s<br>To: %s<br>Leaving at: %i<br>Arriving at: %i", name, timetable[j].stop, timetable[j].destination, timetable[j].leaveTime, timetable[j].arrivalTime);
                                    break;
                                }
                            }
                        }
                        if (isAvaliable == 0) {
                            sprintf(finalResult, "Its too late for a trip today, try again earlier tomorrow ;(");
                        }
                        
                        // REPLYING TO TCP WITH EITHER SUCCESS OR FAILURE BASED ON ADJACENCY
                        char returnToTCP[MAXDATASIZE];
                        snprintf(returnToTCP, MAXDATASIZE*sizeof(char), "%s%s%s%s%s", headerResponse, htmlHeader, finalResult, htmlEnder, goBack);
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
                                    snprintf(request, MAXDATASIZE*sizeof(char), "PATH:%s:%i:%i:%s-%i-%i-%s", destinationStation, timetable[j].arrivalTime, onTransportTime, name, onTransportTime, timetable[j].leaveTime, timetable[j].stop);
                                    // FORMAT OF PATH REQUEST -> PATH:destination:arriveAtDestination:timeOnTransport:thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation
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
                        snprintf(returnToTCP, 2*MAXDATASIZE*sizeof(char), "%s%s%s%s%s", headerResponse, htmlHeader, finalResult, htmlEnder, goBack);
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

                // We only send messages forward if they are faster (as in if it gets to this stop faster, then it will be a faster journey to the destination, conversely if the message is slower to this stop, it will not be a better path)
                char *destOfShortestPath = strtok(shortestPath, "-");
                if ((totalTransportTime <= shortestPathTime) || (destOfShortestPath != destinationStation)) { // Here we are either saying that it is the shortest path of what was flowing through the system before, or its a new request and then we need to start it from scratch
                    shortestPathTime = totalTransportTime; // Make this the new shortest path, this should always be the case for the first option because the longest path is 
                    bzero(shortestPath, sizeof(shortestPath)); // Empty the shortest path 
                    snprintf(shortestPath, MAXDATASIZE*sizeof(char), "%s", otherStepsForReply); // Fill shortest path with this path, we can use this to stop returning messages from being sent back if a more efficient message has passed us

                    int hasArrived = 0; // This will be used to tell if we have arrived at the destination and will change the type of reply

                    char arrayOfVisited[numberOfAdjacentPorts][MAXNAMESIZE]; // An array of stations that are in the history log that is being passed around between the stations
                    //bzero(arrayOfVisited, sizeof(arrayOfVisited)*sizeof(char)); // Empty the array so that we dont have any memory errors 
                    char *stationVisited;
                    stationVisited = strtok(otherSteps, "-"); // This will be the first stop visited based on the path structure (otherSteps is the history and the first part in each history section is the station name)
                    int numberOfStationsVisited = 0; //

                    while (stationVisited != NULL) {
                        snprintf(arrayOfVisited[numberOfStationsVisited], MAXNAMESIZE, "%s", stationVisited); // Since we know it isnt null we can copy it into our array
                        strtok(0,","); // Getting to the next step of the history (each step is seperated by a comma)
                        // bzero(stationVisited, MAXNAMESIZE*sizeof(char)); // Empty the stationVisited variable again so that we dont have any overlapping names
                        stationVisited = strtok(0, "-"); // Reassigning the stationVisited
                        numberOfStationsVisited++; // We have added one more station
                    }

                    char messageToPassForward[MAXDATASIZE]; // This is the message that we will sending in either our PATH or RET message depending on if we are adjacent to the destination
                    //printf("%s\n", messageToPassForward);
                    //bzero(messageToPassForward, sizeof(messageToPassForward)); // Ensuring this is empty (which it should be already)

                    char toBeVisitedStationNameArray[numberOfAdjacentPorts][MAXNAMESIZE]; // This is the array of stations names that are adjacent to us, but are not in this messages history
                    //bzero(toBeVisitedStationNameArray, sizeof(toBeVisitedStationNameArray));
                    int toBeVisitedStationPortArray[numberOfAdjacentPorts]; // This is the array of stations ports that are adjacent to us, but are not in this messages history

                    int toBeVisitedArraySize = 0;
                    int badEgg = 0;
                    for (int i = 0; i < numberOfAdjacentPorts; i++) { // Go through all adjacent ports
                        for (int j = 0; j < numberOfStationsVisited; j++) { // Go through all visited ports
                            if (strcmp(arrayOfVisited[j], stationNameArray[i]) == 0) { // If it is adjacent visited then we dont add it to our array
                                badEgg = 1; // this one is has already been visited
                                printf("HERE WE HAVE THE ONE THAT IS THE SAME FOR i=%i, STATION: %s", i, stationNameArray[i]);
                            }
                        }
                        if (badEgg == 0) { // this one has not been visited
                            printf("We are adding %s to be visited\n", stationNameArray[i]);
                            snprintf(toBeVisitedStationNameArray[toBeVisitedArraySize], MAXNAMESIZE, "%s", stationNameArray[i]);
                            toBeVisitedStationPortArray[toBeVisitedArraySize] = stationPortArray[i];
                            toBeVisitedArraySize++;
                        } else {
                            badEgg = 0;
                        }
                    }

                    int amISendingPath = 0;
                    printf("Stations Visitied: %i, Adjacent Ports: %i, Scuffed Array: %i\n", numberOfStationsVisited, numberOfAdjacentPorts, toBeVisitedArraySize);
                    if (toBeVisitedArraySize > 0) {
                        //printf("%s\n", messageToPassForward);
                        for (int i = 0; i < toBeVisitedArraySize; i++) { // We go through all of the adjacent stations that havent seen this message yet
                            //printf("getting there: %s\n", toBeVisitedStationNameArray[i]);
                            for (int j = 0; j<timetablePos; j++) {
                                //printf("closer\n");
                                if (strcmp(timetable[j].destination, toBeVisitedStationNameArray[i]) == 0) { // We want to be able to remove the stations we have already been to on this journey from our stationNameArray
                                    //printf("OH SHIT A TRAINS HERE, but when,%i >? %i\n", timetable[j].leaveTime, arriveHere);
                                    if (timetable[j].leaveTime >= arriveHere) {
                                        // We need to calculate time taken like this because we are sorting the number as a 4 digit number, meaning that if a train leaves at 1250 and arrives at 1303, timeTaken will be 53 minutes instead of 13, which this below will fix
                                        int timeTaken = timeDif(timetable[j].arrivalTime, timetable[j].leaveTime);
                                        printf("destinationStation: %s\n", destinationStation);
                                        if (strcmp(timetable[j].destination, destinationStation) == 0) {
                                            // RET means that it is returning the path and it was sucessful
                                            snprintf(messageToPassForward, MAXDATASIZE*sizeof(char), "SRET:%i:%s:%i:%i:%s,%s-%i-%i-%s", numberOfStationsVisited-1, destinationStation, timetable[j].arrivalTime, totalTransportTime+timeTaken, otherStepsForReply, name, timeTaken, timetable[j].leaveTime, timetable[j].stop);
                                            printf("Adjacent to destination. Sending: %s back to: %s (port: %i) for checking!!!\n", messageToPassForward, stationNameArray[i], stationPortArray[i]);
                                            hasArrived = 1;
                                            break;
                                        } else {
                                            printf("stationThatIsntDestination: %s\n", toBeVisitedStationNameArray[i]);
                                            snprintf(messageToPassForward, MAXDATASIZE*sizeof(char), "PATH:%s:%i:%i:%s,%s-%i-%i-%s", destinationStation, timetable[j].arrivalTime, totalTransportTime+timeTaken, otherStepsForReply, name, timeTaken, timetable[j].leaveTime, timetable[j].stop);
                                            // FORMAT OF PATH REQUEST -> PATH:destination:arriveAtDestination:timeOnTransport:history,thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation
                                            printf("Request: %s, passed forward to: %s (port: %i)\n", messageToPassForward, toBeVisitedStationNameArray[i], toBeVisitedStationPortArray[i]);
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
                                        if (strcmp(timetable[j].destination, destinationStation) == 0) {
                                            snprintf(messageToPassForward, MAXDATASIZE*sizeof(char), "FRET:%i:%s:%i:%i:%s,%s-%i-%i-%s", numberOfStationsVisited-1, destinationStation, 0, 24*60, otherStepsForReply, name, 0, 0, "NA");
                                            printf("Adjacent to destination, but there are no journeys. Sending: %s back to: %s (port: %i) for checking!!!\n", messageToPassForward, stationNameArray[i], stationPortArray[i]);
                                            hasArrived = 1;
                                        }
                                    }
                                }
                            }

                            if (hasArrived == 1) { // This is a reply
                                for (int i = 0; i < numberOfAdjacentPorts; i++) {
                                    if (strcmp(arrayOfVisited[numberOfStationsVisited-1], stationNameArray[i]) == 0) {
                                        udpSend(stationPortArray[i], messageToPassForward); // This will pass the message back to the previous station
                                        break;
                                    }
                                }
                                hasArrived = 0;
                            } else if (amISendingPath == 1) { // This is a path request
                                printf("I should be sending a path request right now, \nHere: %s\n", messageToPassForward);
                                udpSend(toBeVisitedStationPortArray[i], messageToPassForward);
                                amISendingPath = 0;
                            }
                        }
                    } else {                                            
                        // We will start returning at this point, since we cant go any further
                        for (int i = 0; i < numberOfAdjacentPorts; i++) {
                            if (strcmp(stationNameArray[i],arrayOfVisited[numberOfStationsVisited-1])) {
                                snprintf(messageToPassForward, MAXDATASIZE*sizeof(char), "FRET:%i:%s:%i:%i:%s,%s-%i-%i-%s", numberOfStationsVisited-1, destinationStation, 0, 24*60, otherStepsForReply, name, 0, 0, "NA");
                                udpSend(stationPortArray[i], messageToPassForward);
                                break;
                            }                            
                        }
                    }
                } else { // This is the case where we didnt get a faster response, in this case we could return failed path, not sure yet

                }
            }

            // THIS IS A RETURN REQUEST RECIEVED
            regexCheck = regcomp(&regex, "RET:", 0);
            regexCheck = regexec(&regex, buf, 0, NULL, 0); // This will check if its a return message, and then will sort it into successful and failed returns
            if (regexCheck == 0) {
                printf("Regex Check Success: RET\n");
                
                int isSuccess = 0;
                regexCheck = regcomp(&regex, "SRET:", 0);
                regexCheck = regexec(&regex, buf, 0, NULL, 0); // This means the return was a success, and we will treat it accordingly
                if (regexCheck == 0) {
                    isSuccess = 1; // If this is not the case, then it is a failure (because we will get an FRET)
                }
                
                
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

                printf("I have recieved %i replies, and I explect %i replies\n", numberOfRepliesRecived, numberYouWillRecieveFrom);
                
                /*
                Here we will need to check if we have got all of the replies we should get, and if this is the case, we will send the one best reply to whoever needs it, and failures to everyone else
                */
                if ((numberOfRepliesRecived >= numberYouWillRecieveFrom) || isSuccess) { // Shouldnt need the greater than but just as a fail safe
                    // Here we have recieved all replies that we will need (hopefully), and we can reply with the appropriate replies with success or failure
                    char messageToPassBack[MAXDATASIZE]; // Initialising it for passing back the message
                    //bzero(messageToPassBack, sizeof(messageToPassBack));

                    char currReply[MAXDATASIZE];
                    snprintf(currReply, MAXDATASIZE*sizeof(char), "%s", repliesRecieved[0]);
                    int bestTime = 24*60;
                    int bestReplyInt = 0;
                    int doWeHaveAnSReply = 0;
                    printf("Going through %i replies\n", numberOfRepliesRecived);
                    for (int i; i < numberOfRepliesRecived; i++) {
                        char *dummyReply = strdup(repliesRecieved[i]);
                        printf("Reply %i: %s\n", i, dummyReply);fflush(stdout);
                        
                        regexCheck = regcomp(&regex, "SRET:", 0);
                        regexCheck = regexec(&regex, dummyReply, 0, NULL, 0); // This will check if its a return message, and then will sort it into successful and failed returns             
                        if (regexCheck == 0) {
                            printf("We are in\n");fflush(stdout);
                            doWeHaveAnSReply = 1;
                            strtok(dummyReply, ":"); strtok(0,":"); strtok(0, ":"); strtok(0, ":"); // Get rid of everything we dont need
                            int totalTransportTime = atoi(strtok(0,":")); // Total time on transport so far (which we are using for optimising path)
                            if ((totalTransportTime < bestTime) && (totalTransportTime != 0)) { // we are passing through transport time of zero when it is a failed response and we dont want this to be the min transport time
                                bestTime = totalTransportTime;
                                bestReplyInt = i;
                            }
                        }
                        printf("We have completed %i loops\n", i+1);fflush(stdout);
                    }

                    // --- THERE ARE ALL THE CHARACTERISTICS OF THE CHARACTERISTICS OF THE BEST ROUTE ---
                    printf("LOOK 2Reply %i: %s\n", bestReplyInt, repliesRecieved[bestReplyInt]);fflush(stdout);
                    
                    char *bestReply = strdup(repliesRecieved[bestReplyInt]);
                    int numberOfStationsLeft = atoi(strtok(0,":")); // This is the amount of stations visited in the return path left from this station (just added here for convinence/efficiency)
                    char *destinationStation = strtok(0, ":");
                    int arriveHere = atoi(strtok(0, ":")); // Time it arrives at this station
                    int totalTransportTime = atoi(strtok(0,":")); // Total time on transport so far (which we are using for optimising path)
                    char *otherSteps = strtok(0, ":"); // This is the first step in the journey, we can use this to tell if we are finished and other things
                    char *otherStepsForReply = strdup(otherSteps);

                    printf("LOOK 3Reply %i: %s\n", bestReplyInt, repliesRecieved[bestReplyInt]);fflush(stdout);
                    printf("Breaking down the return message, noLeft: %i, dest: %s, arrHere: %i, timeTot: %i, otherSteps: %s\n",numberOfStationsLeft, destinationStation, arriveHere, totalTransportTime, otherSteps);fflush(stdout);

                    char arrayOfVisited[numberOfAdjacentPorts][MAXNAMESIZE]; // Array needed to keep track of where we have been
                    //bzero(arrayOfVisited, numberOfAdjacentPorts*MAXNAMESIZE*sizeof(char));
                    char *stationVisited = strtok(otherStepsForReply, "-");
                    int numberOfStationsVisited = 0;
                    printf("endering\n");fflush(stdout);
                    while (stationVisited != NULL) {
                        printf("LOOK n: %i Reply %i: %s\n", bestReplyInt, numberOfStationsVisited, repliesRecieved[bestReplyInt]);fflush(stdout);
                        printf("thingo: %s, i:%i\n", stationVisited, numberOfStationsVisited);fflush(stdout);
                        snprintf(arrayOfVisited[numberOfStationsVisited], MAXNAMESIZE*sizeof(char), "%s", stationVisited);
                        printf("LOOK after now snprintf: Best Reply, %i: %s, Reply %i: %s\n", bestReplyInt, repliesRecieved[bestReplyInt], numberOfStationsVisited, repliesRecieved[numberOfStationsVisited]);fflush(stdout);
                        strtok(0,","); //go to next station
                        printf("LOOK after now first strtokn: Best Reply, %i: %s, Reply %i: %s\n", bestReplyInt, repliesRecieved[bestReplyInt], numberOfStationsVisited, repliesRecieved[numberOfStationsVisited]);fflush(stdout);
                        stationVisited = strtok(0, "-"); // station is the first in each "history" section of the reply
                        printf("LOOK after now second strtokn: Best Reply, %i: %s, Reply %i: %s\n", bestReplyInt, repliesRecieved[bestReplyInt], numberOfStationsVisited, repliesRecieved[numberOfStationsVisited]);fflush(stdout);
                        numberOfStationsVisited++;
                    }
                    // ----------------------------------------------------------------------------------
                    printf("hmm\n");fflush(stdout);
                    if (numberOfStationsLeft > 0) { // We arent at the back to source yet, but we will send all our replies
                        printf("LOOK 4Reply %i: %s\n", bestReplyInt, repliesRecieved[bestReplyInt]);fflush(stdout);
                        printf("We arent there yet, getting there\n");fflush(stdout);
                        for (int i = 0; i < numberOfRepliesRecived; i++) {
                            printf("REPLY %i: %s", i, repliesRecieved[i]);fflush(stdout);
                            strtok(repliesRecieved[i], ":"); // This is to remove RET
                            int myNumberOfStationsLeft = atoi(strtok(0,":")); // This is the amount of stations visited in the return path left from this station (just added here for convinence/efficiency)
                            printf("hope");fflush(stdout);
                            strtok(0, ":"); strtok(0, ":"); strtok(0,":"); // Removing unnecessary variables
                            printf("fully\n");fflush(stdout);
                            char *myOtherSteps = strtok(0, ":"); // This is the first step in the journey, we can use this to tell if we are finished and other things
                            char *myOtherStepsForReply = strdup(myOtherSteps);
                            printf("myOtherSteps: %s\n", myOtherSteps);fflush(stdout);

                            char myArrayOfVisited[numberOfAdjacentPorts][MAXNAMESIZE]; // Array needed to keep track of where we have been
                            char *myStationVisited = strtok(myOtherSteps, "-");
                            int myNumberOfStationsVisited = 0;
                            while (myStationVisited != NULL) {
                                snprintf(myArrayOfVisited[myNumberOfStationsVisited], MAXNAMESIZE, "%s", myStationVisited);
                                strtok(0,","); 
                                myStationVisited = strtok(0, "-");
                                myNumberOfStationsVisited++;
                            }

                            if ((i == bestReplyInt) && (doWeHaveAnSReply == 1)) {
                                snprintf(messageToPassBack, MAXDATASIZE*sizeof(char), "SRET:%i:%s:%i:%i:%s", numberOfStationsLeft-1, destinationStation, arriveHere, totalTransportTime, otherStepsForReply);
                            } else {
                                snprintf(messageToPassBack, MAXDATASIZE*sizeof(char), "FRET:%i:%s:%i:%i:%s", numberOfStationsVisited-1, destinationStation, 0, 24*60, myOtherStepsForReply); // this other steps will be the best other steps
                            }

                            for (int j = 0; j < numberOfAdjacentPorts; j++) {
                                if (strcmp(arrayOfVisited[myNumberOfStationsLeft-1], stationNameArray[i]) == 0) {
                                    udpSend(stationPortArray[i], messageToPassBack); // This will pass the message back to the previous station
                                }
                            }
                        }
                    } else {
                        printf("Home time!\n");fflush(stdout);
                        // We are the final stop, our message has come back, now we want to process it 

                        printf("Choo choo we are home, this path will take %i, and our quickest path is %i\n", totalTransportTime, shortestPathTime);                 

                        char *otherStepsFinalCopy = strdup(otherSteps);
                        char *stepsHappening = strtok(otherStepsFinalCopy, ":");
                        while (stepsHappening != NULL) {
                            printf("%s\n", stepsHappening);
                            stepsHappening = strtok(0, ",");
                        }

                        // is the reply a success
                        if (doWeHaveAnSReply == 1) {
                            // this needs to be expanded
                            sprintf(finalResult, "%s, you will arrive at %s, at %i.<br>It took %i minutes (on transport) to get here<br>", otherSteps, destinationStation, arriveHere, totalTransportTime);
                        } else {
                            sprintf(finalResult, "%s", "There is no avaliable path left tonight");
                        }

                        printf("Choo choo we are home via: %s\n", finalResult);

                        char returnToTCP[MAXDATASIZE];
                        snprintf(returnToTCP, MAXDATASIZE*sizeof(char), "%s%s%s%s%s", headerResponse, htmlHeader, finalResult, htmlEnder, goBack);
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
                
            }

            // THIS IS A IAM REQUEST RECIEVED (A REPLY)
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
            close(connFD);
        }
    }
}   