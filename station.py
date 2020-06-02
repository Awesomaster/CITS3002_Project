# CITS3002 Project
# By Josh Collier
# Written and tested in linux

import datetime
import re
import socket
import select
import sys
import time


# -------------------- <INITIALISING VARIABLES> --------------------------
# Max data size
MAXDATASIZE = 1024

# Initialise an empty dictionary to store timetable data
timetableDict = {}

class TimetableEvent:
    def __init__(self, leave, arrive, vehicle, stop):
        self.leave = leave
        self.arrive = arrive
        self.vehicle = vehicle
        self.stop = stop


# Putting values in a more readable format (similar to c)
argv = sys.argv
argc = len(argv)-1

# Store initial values
stationName = argv[1] # Keep as a string
tcpPort = int(argv[2]) # Store as an int
udpPort = int(argv[3]) # Store as an int
adjacentPorts = argv[4:] # This will set the rest of the values inputted as the adjacent ports
for i in range(len(adjacentPorts)):
    adjacentPorts[i] = int(adjacentPorts[i]) # Just to storing them all as ints
adjacentStationDict = {} # Dictionary of all adjacent stations and their port
requestsSent = 0
requestsRecieved = 0
clientsAwaitingReplies = {}
# -------------------- </INITIALISING VARIABLES> --------------------------


# ---------------------- <DEFINING FUNCTIONS> ----------------------------
# Fills out the timetable dictionary, is recalled every time we get a UDP recv, so that we always have the most updated information
def createTimetable():
    timetableDict.clear()
    fileName = "tt-" + stationName
    timetableFile = open(fileName, "r")
    n = 0
    for line in timetableFile:
        if (n > 0):
            timeFileLine = line.split(",") #line is each line of the file
            
            # All the values in a particular line of the timetable
            leaveTime = timeFileLine[0]
            leaveTimeHrMin = leaveTime.split(":")
            leaveTimeHr = int(leaveTimeHrMin[0])
            leaveTimeMin = int(leaveTimeHrMin[1])
            leaveTimeInt = leaveTimeHr*100 + leaveTimeMin

            vehicle = timeFileLine[1]
            stop = timeFileLine[2]
            arriveTime = timeFileLine[3]
            arriveTimeHrMin = arriveTime.split(":")
            arriveTimeHr = int(arriveTimeHrMin[0])
            arriveTimeMin = int(arriveTimeHrMin[1])
            arriveTimeInt = arriveTimeHr*100 + arriveTimeMin

            dest = timeFileLine[4].strip()

            # We will use this as our reference for the order of values for the each timetable slot
            timetableVal = TimetableEvent(leaveTimeInt, arriveTimeInt, vehicle, stop)

            if (dest in timetableDict):
                # if this is a station we have already seen, we are appending the current info to the list we already have, in order
                timetableDict[dest].append(timetableVal)
            else:
                # if this is a new station, we will give it a new dict key, which will be a list that contains lists of important info for each trip (they will be in order since we read line by line and lists keep order)
                timetableDict[dest] = [timetableVal]  
        n += 1      
    timetableFile.close()
    # END OF CREATETIMETABLE FUNC

# Send to a particular port
def udpSend(port, message):
    # https://kite.com/python/answers/how-to-send-a-udp-packet-in-python
    byteMsg = bytes(message, "utf-8")
    sendingSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sendingSocket.sendto(byteMsg, ("localhost", port))
    print("We just sent " + message + " to " + str(port))
    sendingSocket.close()

# Send to a list of ports
def broadcast(portsList, message):
    for port in portsList:
        udpSend(port, message)

# Finding actual time difference between 2 times represented as 4 digit numbers, returned in minutes
def timeDif(arr, leave):
    arrHrs = int(arr/100)
    leaveHrs = int(leave/100)
    arrMins = int(arr%100)
    leaveMins = int(leave%100)
    thisSegmentTime = (arrHrs-leaveHrs)*60 + arrMins - leaveMins
    return thisSegmentTime            

# Inputing ugly 4 digit time number, returning it as a pretty string
def prettyTime(time):
    timeHrs = int(time/100)
    timeMins = int(time%100)
    timeType = ""
    if (timeHrs >= 12):
        if (timeHrs > 12):
            timeHrs -= 12 
        timeType = "PM"
    else:
        timeType = "AM"  
           
    pretty = str(timeHrs) + ":" + str(timeMins) + timeType
    return pretty

# Deal with incoming TCP connection
def tcpListen(sock, udp):
    # Get current time (and convert to the format we are using it in, as a 4 digit number)
    currentTime = datetime.datetime.now()
    currentTimeInt = currentTime.hour*100 + currentTime.minute
    
    # Accept client socket and recieve from them
    client, address = sock.accept()
    data = client.recv(MAXDATASIZE)
    dataStr = data.decode("utf-8") # Data recieved from socket is in byte format, we need to decode it
    
    # Check if this is a journey request (otherwise we will just ignore it)
    if ("GET /?to=" in dataStr):
        # https://docs.python.org/3/library/re.html
        destinationStation = re.search(r'(?<=\?to=)\w+', dataStr) # This will grab the name after the ?to=
        destinationStation = destinationStation.group(0)
        print(destinationStation)
        # NOTE FIX THIS IT ISNT ACTUALLY GETTING

        if (destinationStation in adjacentStationDict.keys()):
            # We are adjacent to the destination and can handle everything in here, yay!!
            isTripAvaliable = False
            avaliableTrips = timetableDict.get(destinationStation)
            returnPath = ""
            for trip in avaliableTrips:
                #print(",".join(trip), "is a possible journey, if its after current time")
                if (trip.leave > currentTimeInt):
                    isTripAvaliable = True
                    returnPath = trip
                    break
                    # We can reply to the TCP request with a successful trip!
            
            reply = "HTTP/1.1 200 OK\nContent-Type: text/html\nConnection: Closed\n\n<html>\n<body>"
            # If we get to this point and we dont have a successful trip, we dont have a path avaliable tonight :(
            if (isTripAvaliable):
                # Return success
                leaveTime = prettyTime(trip.leave)
                arriveTime = prettyTime(trip.arrive)
                reply += "<h2>From: " + stationName + " hop on " + trip.vehicle + " at " + trip.stop + "<br>To: " + destinationStation + "<br>Leaving at: " + leaveTime + "<br>Arriving at: " + arriveTime + "</h2"
            else:
                # Return failure
                reply += "<h2>Its too late for a trip today, try again earlier tomorrow ;(</h2>"
            # Finish with reply tail
            reply += "</body>\n</html>\n"

            # Reply to TCP and close
            client.send(reply.encode('utf-8'))
            client.close()

        else:
            # We are not adjacent to the destination
            # Here we can act as a udpServer OR we can just go back and let the UDP server do its UDP serving until it reaches its eventual end
            # Either way we definitely send path requests
            areWeSendingAny = False
            for station in adjacentStationDict.keys(): # This will go through each adjacent station
                for timetableEvent in timetableDict.get(station): # This will go through each timetable event associated with that station
                    if (timetableEvent.leave > currentTimeInt):
                        areWeSendingAny = True
                        timeTaken = timeDif(timetableEvent.arrive, timetableEvent.leave)
                        historySegment = stationName + "-" + str(timeTaken) + "-" + str(timetableEvent.leave) + "-" + timetableEvent.stop + "-" + timetableEvent.vehicle
                        request = "PATH:" + destinationStation + ":" + str(timetableEvent.arrive) + ":" + str(timeTaken) + ":" + historySegment
                        # NOTE: PATH:destination:arriveAtDestination:timeOnTransport:thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation-vehicleType
                        udpSend(adjacentStationDict.get(station), request)
                        # requestsSent += 1
                        break
            if (areWeSendingAny):
                # If we are sending anything then we want to store the client to be able to response later
                if (destinationStation in clientsAwaitingReplies):
                    clientsAwaitingReplies[destinationStation].append(client)
                else:
                    clientsAwaitingReplies[destinationStation] = [client]
            else:
                # If we arent sending anything it means its too late for any trips so we just reply with that and end connection
                reply = "HTTP/1.1 200 OK\nContent-Type: text/html\nConnection: Closed\n\n<html>\n<body>" + "<h2>Its too late for a trip today, try again earlier tomorrow ;(</h2>" + "</body>\n</html>\n"
                client.send(reply.encode('utf-8'))
                client.close()

            #  # Here we start to listen to UDP, and if we get a return case, it will return to here and that will be our reply
            # while(True):
            #     isSuccess, reply = udpListen(udp)
            #     if (isSuccess):
            #         client.send(reply)
            #         client.close()
            #         break
    else: # if its not a journey request we can just close it and ignore it
        client.close()
    
    sock.close()
    # END OF TCPLISTEN FUNC

# Deal with incoming UDP connection
def udpListen(sock):
    data, address = sock.recvfrom(MAXDATASIZE)
    dataStr = str(data, "utf-8")
    dataList = dataStr.split(":") # All my datagrams use colons as a delimiter
    requestType = dataList[0] # Will will use the first term to tell what kind of request it is and thus dictate how we should reply

    if (requestType == "NAME"):
        # Deal with the name request
        print("We recieved a NAME request")

        # Add adjacent station to our dictionary
        nameOfAdjacent = dataList[1]
        portOfAdjacent = int(dataList[2])
        adjacentStationDict[nameOfAdjacent] = portOfAdjacent

        # Reply with our name and port, but with a different request type so that we dont get in an endless loop
        nameReply = "IAM:" + stationName + ":" + str(udpPort)
        udpSend(portOfAdjacent, nameReply)
        print(adjacentStationDict)

    if (requestType == "IAM"):
        # Deal with IAM request
        print("We recieved an IAM request")

        # Add adjacent station to our dictionary
        nameOfAdjacent = dataList[1]
        portOfAdjacent = int(dataList[2])
        adjacentStationDict[nameOfAdjacent] = portOfAdjacent
        print(adjacentStationDict)

    if (requestType == "PATH"):
        createTimetable() # This will make sure if there are any changes to the timetable file they will be accounted for
        # Deal with PATH request
        print("We recieved a PATH request", dataStr)

        # NOTE: PATH:destination:arriveAtDestination:timeOnTransport:thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation-vehicleType
        destination = dataList[1]
        arriveHere = dataList[2]
        timeTravelled = dataList[3]
        pathHistory = dataList[4].split(",")
        arrayOfVisited = []
        for path in pathHistory:
            arrayOfVisited.append(path.split("-")[0])
        print(arrayOfVisited)
        # Here we construct a list of stations that have not been visited by the path and are adjacent
        arrayToBeVisited = []
        dontAdd = False
        for station in adjacentStationDict.keys():
            for otherStation in arrayOfVisited:
                if (station == otherStation):
                    dontAdd = True
            if (dontAdd == False):
                arrayToBeVisited.append(station)
            else:
                dontAdd = False
        
        reply = "" # We define it here for scoping
        replyPort = 0
        haveReq = False
        for station in arrayToBeVisited: # We go through each station
            for timetableEvent in timetableDict.get(station): # We go through each time we go to that station
                if (timetableEvent.leave >= int(arriveHere)): # If there is one after current time, we want to catch it
                    haveReq = True
                    thisSegTime = timeDif(timetableEvent.arrive, timetableEvent.leave)
                    thisSegment = stationName + "-" + str(thisSegTime) + "-" + str(timetableEvent.leave) + "-" + timetableEvent.stop + "-" + timetableEvent.vehicle
                    # thisSegment = thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation-vehicleType
                    if (station == destination): # This is the case that we have a successful trip to the destination and thus SRET
                        print("We are adjacent, sending back SRET to " + arrayOfVisited[-1])
                        reply = "SRET:" + str(len(arrayOfVisited)-1) + ":" + destination + ":" + str(timetableEvent.arrive) + ":" + str(int(timeTravelled) + thisSegTime) + ":" + ",".join(pathHistory) + "," + thisSegment
                        # FORMAT OF SRET REQUEST -> SRET:noStepsBack:destination:arriveAtDestination:timeOnTransport:history,thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation-vehicleType
                        replyPort = adjacentStationDict.get(arrayOfVisited[-1])
                        break
                    else: # This is the case that we have a successful trip to an adjacent port and thus we send PATH req
                        reply = "PATH:" + destination + ":" + str(timetableEvent.arrive) + ":" + str(int(timeTravelled) + thisSegTime) + ":" + ",".join(pathHistory) + "," + thisSegment
                        # FORMAT OF PATH REQUEST -> PATH:destination:arriveAtDestination:timeOnTransport:history,thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation-vehicleType
                        replyPort = adjacentStationDict.get(station)
                        break
            if (haveReq == False): # This is the case that whether we were adjacent or not, we cannot find a path to keep going and thus we return failure
                reply = "FRET:" + str(len(arrayOfVisited)-1) + ":" + destination + ":" + "0" + ":" + str(24*60) + ":" + ",".join(pathHistory) + "," + stationName + "-" + str(0) + "-" + str(0) + "-NA-NA"
                replyPort = adjacentStationDict.get(arrayOfVisited[-1])
            # We send our reply to the right person
            udpSend(replyPort, reply)

    if (requestType[1:] == "RET"):
        # Deal with RET request
        print("We recieved a RET request", dataStr)

        stepsLeft = dataList[1]
        destination = dataList[2]
        arriveHere = dataList[3]
        timeTravelled = dataList[4]
        pathHistory = dataList[5].split(",")
        arrayOfVisited = []
        for path in pathHistory:
            arrayOfVisited.append(path.split("-")[0])
        print(arrayOfVisited)
        
        if (int(stepsLeft) > 0):
            replyMessage = dataList[0] + ":" + str(int(stepsLeft)-1) + ":" + ":".join(dataList[2:])
            udpSend(adjacentStationDict.get(arrayOfVisited[int(stepsLeft)-1]), replyMessage)
        else:
            reply = "HTTP/1.1 200 OK\nContent-Type: text/html\nConnection: Closed\n\n<html>\n<body>"
            # If we get a successful request
            if (requestType == "SRET"):
                print("celebrations are in order")
                reply += "<h2>You will arrive at " + destination + ", at " + prettyTime(int(arriveHere)) + "<br>It took " + timeTravelled + " minutes (on transport) to get here<br></h2>"

                for path in pathHistory:
                    pathList = path.split("-")
                    currStation = pathList[0]
                    currTimeTaken = pathList[1]
                    currLeaveHere = pathList[2]
                    currStop = pathList[3]
                    currVehicle = pathList[4]
                    reply += "<p>From " + currStation + ", catch the " + currVehicle + " at " + currStop + ". Departing " + prettyTime(int(currLeaveHere)) + ", will take " + currTimeTaken + " minutes.<br></p>\n"
            else:
                print("depression is in order")
                reply += "<h2>Failed path, try again tomorrow :(</h2>"
            
            reply += "</body>\n</html>\n"

            # Reply to TCP and close
            client = clientsAwaitingReplies.get(destination).pop(0)
            client.send(reply.encode('utf-8'))
            client.close()
    
    sock.close()
    # END OF UDPLISTEN FUNC
# ---------------------- </DEFINING FUNCTIONS> ----------------------------


# ---------------------------- <CODE FLOW> ---------------------------------
# Source: https://docs.python.org/3/howto/sockets.html
# Create TCP socket
tcpSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcpSock.bind(('localhost', tcpPort))
tcpSock.listen(1)

# Create UDP socket
udpSock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udpSock.bind(('localhost', udpPort))

# Produce the message that will be sent to all adjacent ports on startup that allows us to build the adjacentStationDict
nameMessage = "NAME:" + stationName + ":" + str(udpPort)
broadcast(adjacentPorts, nameMessage)

createTimetable()
print("TCP & UDP Listening...")
while True:
    # https://stackoverflow.com/questions/5160980/use-select-to-listen-on-both-tcp-and-udp-message
    inputSock, outputSock, exceptSock = select.select([tcpSock, udpSock], [], [])

    for sock in inputSock:
        if sock == tcpSock:
            print(stationName + ": We just heard tcp things")
            tcpListen(sock, udpSock) #TCP stuff
        if sock == udpSock:
            print(stationName + ": We just heard udp things")
            udpListen(sock) #UDP stuff happens
        else:
            print("Socket Unknown: ", sock)
# --------------------------- </CODE FLOW> ---------------------------------