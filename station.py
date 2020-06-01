import socket
import sys
import time
import re
import datetime

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

def createTimetable():
    fileName = "tt-" + stationName
    timetableFile = open(fileName, "r")
    for line in timetableFile:
        timeFileLine = line.split(",") #line is each line of the file
        
        # All the values in a particular line of the timetable
        leaveTime = timeFileLine[0]
        leaveTimeHrMin = timeFileLine.split(":")
        leaveTimeHr = int(leaveTimeHrMin[0])
        leaveTimeMin = int(leaveTimeHrMin[1])
        leaveTimeInt = leaveTimeHr*100 + leaveTimeMin

        vehicle = timeFileLine[1]
        stop = timeFileLine[2]
        arriveTime = timeFileLine[3]
        arriveTimeHrMin = timeFileLine.split(":")
        arriveTimeHr = int(arriveTimeHrMin[0])
        arriveTimeMin = int(arriveTimeHrMin[1])
        arriveTimeInt = arriveTimeHr*100 + arriveTimeMin

        dest = timetableFile[4]

        # We will use this as our reference for the order of values for the each timetable slot
        timetableVal = TimetableEvent(leaveTimeInt, arriveTimeInt, vehicle, stop)

        if (dest in timetableDict):
            # if this is a station we have already seen, we are appending the current info to the list we already have, in order
            timetableDict[dest].append(timetableVal)
        else:
            # if this is a new station, we will give it a new dict key, which will be a list that contains lists of important info for each trip (they will be in order since we read line by line and lists keep order)
            timetableDict[dest] = [timetableVal]
            
    timetableFile.close()
    # END OF CREATETIMETABLE FUNC

# Send to a particular port
def udpSend(port, message):
    # https://kite.com/python/answers/how-to-send-a-udp-packet-in-python
    byteMsg = bytes(message, "utf-8")
    sendingSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sendingSocket.sendto(byteMsg, ("localhost", port))

# Send to a list of ports
def broadcast(portsList, message):
    for port in portsList:
        udpSend(port, message)

# Deal with incoming TCP connection
def tcpListen(sock, udp):
    # Get current time (and convert to the format we are using it in, as a 4 digit number)
    currentTime = datetime.datetime.now()
    currentTimeInt = currentTime.hour*100 + currentTime.minute

    client, address = sock.accept()
    data = client.recv(MAXDATASIZE)
    # deal with it
    
    # Check if this is a journey request
    if (data.contains("GET /?to=")):
        # https://docs.python.org/3/library/re.html
        destinationStation = re.search(r'(?<=\?to=)\w+', data) # This will grab the name after the ?to=

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
                leaveTimeMins = trip.leave%100
                leaveTimeHours = int(trip.leave/100)
                leaveTimeType = ""
                if (leaveTimeHours >= 12):
                    leaveTimeType = "PM"
                    if (leaveTimeHours > 12):
                        leaveTimeHours -= 12
                else:
                    leaveTimeType = "AM"

                arriveTimeMins = trip.arrive%100
                arriveTimeHours = int(trip.arrive/100)
                arriveTimeType = ""
                if (arriveTimeHours >= 12):
                    leaveTimeType = "PM"
                    if (arriveTimeHours > 12):
                        arriveTimeHours -= 12
                else:
                    arriveTimeType = "AM"
                
                reply += "<h2>From: " + stationName + " hop on " + trip.vehicle + " at " + trip.stop + "<br>To: " + destinationStation + "<br>Leaving at: " + str(leaveTimeMins) + " past " + str(leaveTimeHours) + leaveTimeType + "<br>Arriving at: " + str(arriveTimeMins) + " past " + str(arriveTimeHours) + arriveTimeType + "</h2"
            else:
                # Return failure
                reply += "Its too late for a trip today, try again earlier tomorrow ;("
            # Finish with reply tail
            reply += "</body>\n</html>\n"

            # Reply to TCP and close
            client.send(reply)

        else:
            # We are not adjacent to the destination
            # Here we can act as a udpServer OR we can just go back and let the UDP server do its UDP serving until it reaches its eventual end
            # Either way we definitely send path requests
            for station in adjacentStationDict.keys(): # This will go through each adjacent station
                for timetableEvent in timetableDict.get(station): # This will go through each timetable event associated with that station
                    if (timetableEvent.leave > currentTimeInt):
                        timeTaken = timetableEvent.arrive - timetableEvent.leave
                        historySegment = stationName + "-" + str(timeTaken) + "-" + str(timetableEvent.leave) + "-" + timetableEvent.stop + "-" + timetableEvent.vehicle
                        request = "PATH:" + destinationStation + ":" + str(timetableEvent.arrive) + ":" + str(timeTaken) + ":" + historySegment
                        # NOTE: PATH:destination:arriveAtDestination:timeOnTransport:thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation-vehicleType
                        udpSend(adjacentStationDict.get(station), request)
                        requestsSent += 1
                        break
            
             # Here we start to listen to UDP, and if we get a return case, it will return to here and that will be our reply
            while(True):
                isSuccess, reply = udpListen(udp)
                if (isSuccess):
                    client.send(reply)
                    break

    # If we deal with the TCP connection entirely within this tcpListen func we can have this at the end, but its not necessarily necessary
    client.close()

    # END OF TCPLISTEN FUNC

# Deal with incoming UDP connection
def udpListen(sock):
    data, address = sock.recvfrom(MAXDATASIZE)
    dataList = data.split(":") # All my datagrams use colons as a delimiter
    requestType = dataList[0] # Will will use the first term to tell what kind of request it is and thus dictate how we should reply

    if requestType == "NAME":
        # Deal with the name request
        print("We recieved a NAME request")

        # Add adjacent station to our dictionary
        nameOfAdjacent = dataList[1]
        portOfAdjacent = int(dataList[3])
        adjacentStationDict[nameOfAdjacent] = portOfAdjacent

        # Reply with our name and port, but with a different request type so that we dont get in an endless loop
        nameReply = "IAM:" + stationName + ":" + str(udpPort)
        udpSend(portOfAdjacent, nameReply)

    if requestType == "IAM":
        # Deal with IAM request
        print("We recieved an IAM request")

        # Add adjacent station to our dictionary
        nameOfAdjacent = dataList[1]
        portOfAdjacent = int(dataList[3])
        adjacentStationDict[nameOfAdjacent] = portOfAdjacent

    if requestType == "PATH":
        # Deal with PATH request
        print("We recieved a PATH request")

        # NOTE: PATH:destination:arriveAtDestination:timeOnTransport:thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation-vehicleType
        destination = dataList[1]
        arriveAtDest = dataList[2]
        timeTravelled = dataList[3]
        pathHistory = dataList[4].split(",")

    if requestType == "RET":
        # Deal with RET request
        print("We recieved a RET request")

        # If we get a successful request
        successfulReturnPath = ""
        return True, successfulReturnPath
    
    # END OF UDPLISTEN FUNC

# Source: https://docs.python.org/3/howto/sockets.html
# Create TCP socket
tcpSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcpSock.connect(('localhost', tcpPort));

# Create UDP socket
udpSock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udpSock.bind(('localhost', udpPort))

# Produce the message that will be sent to all adjacent ports on startup that allows us to build the adjacentStationDict
nameMessage = "NAME:" + stationName + ":" + str(udpPort)
broadcast(adjacentPorts, nameMessage)

while True:
    # https://stackoverflow.com/questions/5160980/use-select-to-listen-on-both-tcp-and-udp-message
    inputSock, outputSock, exceptSock = socket.select([tcpSock, udpSock], [], [])

    for sock in inputSock:
        if sock == tcpSock:
            tcpListen(sock, udpSock) #TCP stuff
        if sock == udpSock:
            udpListen(sock) #UDP stuff happens
        else:
            print("Socket Unknown: ", sock)