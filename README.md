# CITS3002_Project

This is the github repository for the CITS3002 Project 2020

What we get:

For each station, we run an instance of the code.
Each of these stations will be initialised with a name, a port, an address, and a list of all adjacent stations. Each station will also have an associated text file that include the station name and coordinates as the top line, and is followed by the stations timetable of all departure times, the stop they are at, the arrival time, and the arrival destinations name.


Approach to project:

We are given a station that we want to get to from a station that we start at, presumably at a time we are starting. From here we need to find the station that we are going to, if its not already in our address book, we need to find it, we can do this by passing around one of those packets that has a certain lifetime likely, to find the path from our station to it, and along the way keep an address book of stations ports and names. Once we have this we need to send a signal to the closest stop, asking for its next trip to the next stop along its journey, and this can either be passed on from there or be passed on by the stop its currently at, only to be returned when its at its target destination with all the information.

Something cool to add would be to get your location to find the closest stop, or let you put in your location through interacting with a google maps ui or by giving your lat/long and it at least showing you where that is on google maps to confirm with you, potentially even marking the stops on google maps...