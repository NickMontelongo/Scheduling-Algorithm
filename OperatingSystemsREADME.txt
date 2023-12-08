TO RUN THE FOLLOWING PROGRAM YOU WILL NEED THE FOLLOWING COMMANDS

Command1: g++ -o Program1 main.cpp -std=gnu++11
-This will create a Program1 executable, the -std=gnu++11 flag MUST BE PRESENT OR WILL NOT RUN

./<Executable name> or ./Program1 <scheduleSelector values 1-4> <average Arrival Rate value> <average Service Time Value> <quantum value>
This command will run the executable

ex) ./Program1 1 1 0.6 0.1
This will Run first come first serve with an arrival rate of 1 with an avgServiceTime of .6 time units (quantum is ignored)

*quantum will only be used in round robin scheduleSelector=4