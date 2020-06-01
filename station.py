import socket
import sys
import time

# Putting values in a more readable format (similar to c)
argv = sys.argv
argc = len(argv)-1

# Store initial values
stationName = argv[2]
tcpPort = int(argv[3])
udpPort = int(argv[4])
adjacentPorts = argv[5:] # This will set the rest of the values inputted as the adjacent ports
for i in range(len(adjacentPorts)):
    adjacentPorts[i] = int(adjacentPorts[i]) # Just to set them all to ints

# Source: https://docs.python.org/3/howto/sockets.html
# Create TCP socket
tcpSock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcpSock.connect('localhost', tcpPort);

# Create UDP socket
udpSock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
serverAddress = ('localhost', udpPort)
udpSock.bind(serverAddress)

timetableDict = {}

fileName = "tt-" + stationName
timetableFile = open(fileName, "r")

# <pseudocodeForFile>
timeFileLine = line.split(",") #line is each line of the file
if (!(timetableFile[5] in timetableDict)):
    timetableDict[timetableFile[5]] = timeFileLine[0:4]
else:
    timetableDict[timetableFile[5]].append(timeFileLine[0:4])
leaveTime = timeFileLine[0]
vehicle = timeFileLine[1]
# </pseudocodeForFile>

# <pseudocodeForPath>
pathToSend = "PATH:" + destination + ":" + arriveAtDest + ":" +timeOnTransport + ":" + thisStationName + "-" + timeOfThisSplit + "-" + timeLeaveThisStation + "-" + stop + "-" + vehicle
# FORMAT OF PATH REQUEST -> PATH:destination:arriveAtDestination:timeOnTransport:thisStationName-timeOfThisSplit-timeLeftThisStation-stopAtThisStation-vehicleType
# To break it down
pathList = pathRecieved.split(":")
historyPathList = pathList[-1].split(",")

# </pseudocodeForPath>


print("The script has the name %s\n" % (sys.argv[1]))
time.sleep(10)