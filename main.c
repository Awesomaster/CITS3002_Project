// Starting code for CITS3002 in C

int main(int argc, char **argv) {
    char *name = argv[0]; // the spoken name of the station that will be used to refer to the station
    int port = argv[1]; // port that this station is accessed from e.g. http://localhost:port
    int address = argv[2]; // address for other stations to use to communicate with this station 

    char *adjacentStations = argv; // we will be using all arguments argv[3:argc]

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