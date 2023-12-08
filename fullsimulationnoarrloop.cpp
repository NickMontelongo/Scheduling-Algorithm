#include <iostream>
#include <random>
#include <vector>
#include <math.h>
#include <algorithm>
#include <cstdlib>
using namespace std;

//create an event to sample the queue size
//better than sampling the queue when events

enum eventType {
    ARRIVAL = 0,
    DEPARTURE = 1,
    TIMESLICE = 2,
    SAMPLEREADYQUEUE = 3,
    TENTATIVEDEPARTURE = 4
};


struct process {
    int id;
    float arrivalTime;
    float serviceTime;
    float finishTime;
    float remainingServiceTime;
    process(int processID,float arrival_time, float service_time){
        this->id = processID;
        this->arrivalTime = arrival_time;
        this->serviceTime = service_time;
        this->remainingServiceTime = service_time;
    }
};

//Node for event queue "stores events"
//needs to be sorted based on execution time
struct event {
    //type to be 0 for arrival or 1 for departure
    int type;
    float time;
    event* nextEvent;
    process* processInvolved;
    event(float time, int type, process* processForEvent = NULL){
        this->type = type;
        this->time = time;
        this->processInvolved = processForEvent;
        this->nextEvent = NULL;
    }
};


//AvgServiceRate and AvgArrivalRates used for calculation
int avgArrivalRate = 0;
float avgServiceTime = 0;
int scheduleSelector = 1;

//Process Variables
vector<process*> finishedProcesses;
vector<process*> processReadyQueue; //queue of processes waiting to be serviced by the cpu, assume single cpu may or may not be sorted
int processIDCounter = 1; //gives id 1-10000+

//Simulation Event Queue, stores Sorted Arrivals and Departures
event* simulationEventQueue;

//State Variables
process* server;
float simulationClock;
int readyQueueCount = 0;

float quantum = 0;
int const TOTALPROCESSES = 10000;
int processToRun = 0;

//Evaluation Variables
float processServerStartTime = 0;
float systemIdleTime = 0;
float averageProcessReadyQueue = 0;
double numberTimesAvgProReadyQueueCalled = 0;

//Calculation Functions Based On Exponential/Poisson Distributions
double calculateServiceTime(float averageServiceTime){
    // generate random uniform value between 0-1
    double uniformValue=((double)rand()/((double)RAND_MAX));
    // while loop to make sure we dont generate 0 as a value for calculations
    while (uniformValue <= 0.0) {
        uniformValue=((double)rand()/((double)RAND_MAX));
    }
    //service times by x = -t * ln(z) where z is uniform 0-1
    double service_time = -1.0 * averageServiceTime *(log(uniformValue));
    return service_time;
}

double calculateArrivalTime(int arrival_rate){
    // generate random uniform value between 0-1
    double uniformValue=((double)rand()/((double)RAND_MAX));
    while (uniformValue <= 0.0) {
        uniformValue=((double)rand()/((double)RAND_MAX));
    }
    //inter-arrival times by x = -1/arrival_time * ln(z) where z is uniform 0-1
    double Arrival_time = -(1.0/arrival_rate) *(log(uniformValue));
    return Arrival_time;
}

//Schedulers Present (Round Robin is implemented in the supporting arrival/departure/timeslice handlers
process* firstComeFirstServe(vector<process*> processReadyQueue){
    float lowestArrivalTime = processReadyQueue[0]->arrivalTime;
    int processIndex = 0;
    for(int queueCounter = 1; queueCounter < processReadyQueue.size(); queueCounter++){
        if(processReadyQueue[queueCounter]->arrivalTime < lowestArrivalTime){
            lowestArrivalTime = processReadyQueue[queueCounter]->arrivalTime;
        }
    }
    return processReadyQueue[processIndex];
}


process* highestResponseRatioNext(vector<process*> processReadyQueue){
    int highestRRProcessIndex = 0;
    float highestResponseRatio = 0;
    for(int queueCounter = 0; queueCounter < processReadyQueue.size(); queueCounter++){
      float currentProcessWaitTime = simulationClock - processReadyQueue[queueCounter]->arrivalTime;
      float currentProcessServiceTime = processReadyQueue[queueCounter]->serviceTime;
      float currentProcessResponseRatio = (currentProcessWaitTime + currentProcessServiceTime) / currentProcessServiceTime;
      if(currentProcessResponseRatio > highestResponseRatio){
         highestResponseRatio = currentProcessResponseRatio;
         highestRRProcessIndex = queueCounter;
        }
    }
    return processReadyQueue[highestRRProcessIndex];
}


process* shortestRemainingTimeFirst(vector<process*> processReadyQueue){
    float shortestRemainingTime = processReadyQueue[0]->remainingServiceTime;
    int processIndex = 0;
    for(int queueCounter = 1; queueCounter < processReadyQueue.size(); queueCounter++){
        if(processReadyQueue[queueCounter]->remainingServiceTime < shortestRemainingTime){
            shortestRemainingTime = processReadyQueue[queueCounter] -> remainingServiceTime;
            processIndex = queueCounter;
        }
    }
    return processReadyQueue[processIndex];
}


//Additional Functions for Use
event* returnNextArrivalEventTime(event* eventQueue){
    //assert(eventQueue != NULL);
    event* nextArrivalEvent = NULL;
    event* current = eventQueue;
    while(current != nullptr && current->type != 0){
        current = current->nextEvent;
    }
    if (current->type == 0){
        nextArrivalEvent = current;
    }
    return nextArrivalEvent;
}


void scheduleEvent(int eventType, float eventTime, event*& eventqueueHead, process* newProcess = NULL){
    //create a new process for each arrival
    //if event is an arrival we attach new process to that event
    if(eventType == ARRIVAL){
        //calculate processes service rate
        float processServiceTime = calculateServiceTime(avgServiceTime);
        newProcess = new process(processIDCounter, eventTime, processServiceTime);
        //increment process counter for next creation
        processIDCounter++;
    }
    event* newEvent = new event(eventTime, eventType, newProcess);
    if (eventqueueHead == NULL) {
        eventqueueHead = newEvent;
    }
    else if(newEvent->time < eventqueueHead->time) {
        newEvent->nextEvent = eventqueueHead;
        eventqueueHead = newEvent;
    }
    else {
        event *current = eventqueueHead;
        event *prev = nullptr;
        while(current->nextEvent != nullptr && current->time <= newEvent->time ) {
            prev = current;
            current = current->nextEvent;
        }
        if(newEvent->time < current->time) {
            prev->nextEvent = newEvent;
            newEvent->nextEvent = current;
        }
        else {
            current->nextEvent = newEvent;
        }
    }
}

//HANDLER FUNCTIONS
void sampleReadyQueueHandler(vector<process*> processReadyQueue, event*& eventqueueHead){
    int currentSizeOfQueue = processReadyQueue.size();
    averageProcessReadyQueue = averageProcessReadyQueue + currentSizeOfQueue;
    numberTimesAvgProReadyQueueCalled++;
    float eventTime = simulationClock + .003;
    scheduleEvent(SAMPLEREADYQUEUE, eventTime, eventqueueHead, NULL);


}


void tentativeDepartureHander(vector<process*> &processReadyQueue, event*&eventqueueHead){
    //LOOKS AT THE DEPARTURE EVENT, if eventqueue->processInvolved == SERVER is TRUE
    //Then we perform the tentative departure as a departure event ie can carry out
    //otherwise tentative departure is invalid
    if(eventqueueHead->processInvolved == server){
        float eventTime = simulationClock;
        scheduleEvent(DEPARTURE,eventTime, eventqueueHead, server);
    }
}


void timeSliceHandler(vector<process*> &processReadyQueue, event*&eventqueue){
    processReadyQueue.push_back(server);
    //get next process which is the head of the process readyqueue FIFO
    server = processReadyQueue[0];
    //Delete Pre-existing head since we will need to push_back to the end once timeslice is handled
    processReadyQueue.erase(processReadyQueue.begin());
    //CASE1: quantum is greater than/equal to remainingServiceTime so we schedule its departure
    if (quantum >= server->remainingServiceTime){
        float newDepartureTime = simulationClock + server->remainingServiceTime;
        scheduleEvent(DEPARTURE, newDepartureTime, eventqueue, eventqueue->processInvolved);
    }
    else{
        //CASE2: quantum is less than remainingServiceTime, so we deduct it from remainingServiceTime
        server->remainingServiceTime = server->remainingServiceTime - quantum;
        float newTimeSliceTime = simulationClock + quantum;
        //move simulationClock forward by the new ammount, simulationClock (current time) + quantum
        scheduleEvent(TIMESLICE, newTimeSliceTime, eventqueue, server);
    }
}


void arrivalHandlerNonPreemptive(event*& eventqueue, int avgArrivalRate, float avgServiceTime){
    if (server == NULL){
        server = eventqueue->processInvolved;
        float newDepartureTime= simulationClock + server->serviceTime;
        //schedule the new departure for the first process, add process to departure event
        scheduleEvent(DEPARTURE,newDepartureTime, eventqueue, server);
    }
    else{
        readyQueueCount++;
        //add process from attached arrival to process ready queue
        processReadyQueue.push_back(eventqueue->processInvolved);
    }
    float nextProcessArrivalTime = simulationClock + calculateArrivalTime(avgArrivalRate);
    scheduleEvent(ARRIVAL, nextProcessArrivalTime, eventqueue);
}


void departureHandlerNonPreemptive(event*& eventqueue, float serviceTime, int scheduleSelector){
    if (processReadyQueue.empty()){
        server = NULL;
        eventqueue->processInvolved->finishTime = simulationClock ;
        finishedProcesses.push_back(eventqueue->processInvolved);
    }
    else {
        //search for next process in processReadyQueue according to closest service time relative to simulationClock
        readyQueueCount--;
        //log deleted process and its finish time
        eventqueue->processInvolved->finishTime = simulationClock;
        finishedProcesses.push_back(eventqueue->processInvolved);
        process* processToDelete = NULL;
        if (scheduleSelector == 1){
            processToDelete = firstComeFirstServe(processReadyQueue);
        }
        if (scheduleSelector == 3){
            processToDelete = highestResponseRatioNext(processReadyQueue);
        }
        float newDepartureTime= simulationClock + processToDelete->serviceTime;
        scheduleEvent(DEPARTURE, newDepartureTime, eventqueue, processToDelete);
        auto it = std::find(processReadyQueue.begin(), processReadyQueue.end(), processToDelete);
        if(it != processReadyQueue.end())
            processReadyQueue.erase(it);

    }
}


void arrivalHandlerSRTF(event*& eventqueue, int avgArrivalRate, float avgServiceTime){
    //CASE1: server points to process being worked on
    // if NULL, nothing going on so run Process ie is only Process to Run
    if (server == NULL){
        //set the process to be running on server and record the initial time it runs
        server = eventqueue->processInvolved;
        processServerStartTime = simulationClock;
        float newDepartureTime= simulationClock + eventqueue->processInvolved->remainingServiceTime;
        //schedule the new departure for the first process, add process to departure event
        scheduleEvent(TENTATIVEDEPARTURE,newDepartureTime, eventqueue, eventqueue->processInvolved);
    }
    //CASE2: Another Process is running currently, so we *Possibly* add to the processReadyqueue
    else{
        //CASE2A: New Process has a smaller service Time than the current process
        if (eventqueue->processInvolved->remainingServiceTime < server->remainingServiceTime){
            //adjust the time for the pointer server is pointing to
            server->remainingServiceTime = server->remainingServiceTime - (simulationClock - processServerStartTime);
            processReadyQueue.push_back(server);
            //switch to servicing incoming process
            server = eventqueue->processInvolved;
            processServerStartTime = simulationClock;
            float newDepartureTime = simulationClock + server->remainingServiceTime;
            scheduleEvent(TENTATIVEDEPARTURE, newDepartureTime, eventqueue, server);

        }
        else{
        //CASE2B: Otherwise Continue Running and add process to readyqueue
        readyQueueCount++;
        //add process from attached arrival to process ready queue
        processReadyQueue.push_back(eventqueue->processInvolved);
        }
    }
    float nextProcessArrivalTime = simulationClock + calculateArrivalTime(avgArrivalRate);
    scheduleEvent(ARRIVAL, nextProcessArrivalTime, eventqueue);
    //ACTIVE TIMEKEEPING SECTION
    event* nextArrivalEvent = returnNextArrivalEventTime(eventqueue);
    if (server->remainingServiceTime > nextArrivalEvent->processInvolved->arrivalTime){
        server->remainingServiceTime = server->remainingServiceTime - nextArrivalEvent->processInvolved->arrivalTime;
    }
    else{
        server->remainingServiceTime = 0;
    }
}


void departureHandlerSRTF(event*& eventqueue, float serviceTime){
    if (processReadyQueue.empty()){
        eventqueue->processInvolved->finishTime = simulationClock;
        finishedProcesses.push_back(eventqueue->processInvolved);
        server = NULL;
    }
    else {
        //search for next process in processReadyQueue according to closest service time relative to simulationClock
        readyQueueCount--;
        //log deleted process and its finish time
        server->finishTime = simulationClock;
        finishedProcesses.push_back(server);
        server = shortestRemainingTimeFirst(processReadyQueue);
        auto it = std::find(processReadyQueue.begin(), processReadyQueue.end(), server);
        if(it != processReadyQueue.end()){
            processReadyQueue.erase(it);
            }
        processServerStartTime = simulationClock;
        float newDepartureTime= simulationClock + server->remainingServiceTime;
        scheduleEvent(TENTATIVEDEPARTURE, newDepartureTime, eventqueue, server);

    }
}


void arrivalHandlerRoundRobin(event*& eventqueue, int avgArrivalRate, float avgServiceTime){
    //Case1: where everything is empty and "first Process Arrives"
    if (server == NULL){
        server = eventqueue->processInvolved;
        //if quantum is bigger than the servicetime needed by the process we schedule the process for completion and continue
        if (quantum >= server->remainingServiceTime){
            float newDepartureTime= simulationClock + server->remainingServiceTime;
            //schedule the new departure for the first process, add process to departure event
            scheduleEvent(DEPARTURE,newDepartureTime, eventqueue, server);
        }
        else{
            //quantum is smaller than process's service time so we deduct the quantum from the service time into remainingServiceTime
            server->remainingServiceTime = server->remainingServiceTime - quantum;
            float newTimeSliceTime = simulationClock + quantum;
            //move simulationClock forward by the new ammount, simulationClock (current time) + quantum
            scheduleEvent(TIMESLICE, newTimeSliceTime, eventqueue, server);
        }
    }
    //Case2: where there are pre-existing processes in ReadyQueue with a process being worked on
    else{
        readyQueueCount++;
        //add process from attached arrival to process ready queue
        processReadyQueue.push_back(eventqueue->processInvolved);
    }
    // Schedule the next new Process Regardless of Case1/2.
    float nextProcessArrivalTime = simulationClock + calculateArrivalTime(avgArrivalRate);
    scheduleEvent(ARRIVAL, nextProcessArrivalTime, eventqueue);
}


void departureHandlerRoundRobin(event*& eventqueue, float serviceTime){
    //CASE 1: ProcessReadyQueue has no Process, current Process Terminates and log information
    if (processReadyQueue.empty()){
        server->finishTime = simulationClock ;
        finishedProcesses.push_back(server);
        //cout << "Process: " << server->id << " is being pushed to finished Processes Case 1. " << endl;
        server = NULL;
    }
    else {
        //search for next process in processReadyQueue according to closest service time relative to simulationClock
        readyQueueCount--;
        //log deleted process and its finish time
        server->finishTime = simulationClock;
        finishedProcesses.push_back(server);
        //Choose next process at head of the readyqueue and give to server to process
        server = processReadyQueue[0];
        processReadyQueue.erase(processReadyQueue.begin());
        if (quantum >= server->remainingServiceTime){
            float newDepartureTime= simulationClock + server->remainingServiceTime;
            //schedule the new departure for the first process, add process to departure event
            scheduleEvent(DEPARTURE,newDepartureTime, eventqueue, server);
        }
        else{
            //quantum is smaller than process's service time so we deduct the quantum from the service time into remainingServiceTime
            server->remainingServiceTime = server->remainingServiceTime - quantum;
            float newTimeSliceTime = simulationClock + quantum;
            //move simulationClock forward by the new ammount, simulationClock (current time) + quantum
            scheduleEvent(TIMESLICE, newTimeSliceTime, eventqueue, server);
        }

    }
}




//SIMULATION FUNCTIONS
void initializeSimulation(int avgArrivalRate) {
        simulationClock = 0;
        server = NULL;
        finishedProcesses.clear();
        processReadyQueue.clear(); //process queue head
        simulationEventQueue = NULL; // event queue head
        float initialProcessArrivalTime = calculateArrivalTime(avgArrivalRate);
        scheduleEvent (SAMPLEREADYQUEUE, simulationClock + .003, simulationEventQueue);
        scheduleEvent (ARRIVAL, initialProcessArrivalTime, simulationEventQueue);
    }


void showResults(float selectedServiceTime, int selectedArrivalRate,int scheduleSelector, float quantum = 0){
    cout << "Average arrival Rate: " << selectedArrivalRate << "  " << "Average Service Time: " << selectedServiceTime << endl;
    if(quantum != 0 and scheduleSelector == 4){
        cout << "Quantum: " << quantum << endl;
    }
    //Average Turnaround Time of Processes
    float sumTurnaroundTime = 0;
    for (int i=0; i < TOTALPROCESSES; i++){
    sumTurnaroundTime = sumTurnaroundTime + (finishedProcesses[i]->finishTime - finishedProcesses[i]->arrivalTime);
    }
    float avgTurnaroundTime = sumTurnaroundTime / TOTALPROCESSES;
    cout << "Average Turnaround Time: " << avgTurnaroundTime << endl;
    //Throughput
    float throughput = TOTALPROCESSES/ simulationClock;
    cout << "Total throughput: " << throughput << endl;
    //Average CPU Utilization
    float avgCPUUtilization = (simulationClock - systemIdleTime)/ simulationClock;
    avgCPUUtilization = avgCPUUtilization * 100;
    cout<< "Average CPU Utilization: " << avgCPUUtilization << " % " << endl;
    //Average Number of Processes in Ready Queue
    float avgNumProcesses = averageProcessReadyQueue/numberTimesAvgProReadyQueueCalled;
    cout<< "Average number of processes in Ready Queue: " << avgNumProcesses << endl;
    cout<< endl << endl;
}

void runSimulation(float avgServiceTime, int avgArrivalRate, int scheduleSelector) {
    while (processToRun < TOTALPROCESSES ) {
        float oldClock = simulationClock;
        simulationClock = simulationEventQueue->time;
        float timeElapsed = simulationClock - oldClock;
        if(server == NULL){
            systemIdleTime = systemIdleTime + timeElapsed;
        }
        switch (simulationEventQueue-> type) {
            case ARRIVAL:
                //record arrival time for process
                if(scheduleSelector == 1 or scheduleSelector == 3){
                    arrivalHandlerNonPreemptive(simulationEventQueue, avgArrivalRate, avgServiceTime);
                }
                else if(scheduleSelector == 2){
                    arrivalHandlerSRTF(simulationEventQueue, avgArrivalRate, avgServiceTime);
                }
                else{
                    arrivalHandlerRoundRobin(simulationEventQueue, avgArrivalRate, avgServiceTime);
                }
                break;
            case DEPARTURE:
                //record departure time for process
                if(scheduleSelector == 1 or scheduleSelector == 3){
                    departureHandlerNonPreemptive(simulationEventQueue, avgServiceTime, scheduleSelector);
                }
                else if(scheduleSelector == 2){
                    departureHandlerSRTF(simulationEventQueue, avgServiceTime);
                }
                else{
                    departureHandlerRoundRobin(simulationEventQueue, avgServiceTime);
                }
                processToRun++;
                break;
            case TIMESLICE:
                timeSliceHandler(processReadyQueue, simulationEventQueue);
                break;
            case SAMPLEREADYQUEUE:
                sampleReadyQueueHandler(processReadyQueue, simulationEventQueue);
                break;
            case TENTATIVEDEPARTURE:
                tentativeDepartureHander(processReadyQueue, simulationEventQueue);
                break;

        }
        //delete event from event queue
        event* headEventToDelete = simulationEventQueue;
        simulationEventQueue = simulationEventQueue->nextEvent;
        delete headEventToDelete;
    }
    showResults(avgServiceTime, avgArrivalRate, scheduleSelector, quantum);
}

void fullSimulation(int scheduleSelector, int avgArrivalRate, float avgServiceTime, float quantum){
    //Display Initial Metrics
    string simulationType[] = {"Placeholder","First Come First Serve", "Shortest Remaining Time First", "Highest Response Ratio Next", "Round Robin"};
    cout << "xxxxxxxxxxxx " << simulationType[scheduleSelector] << " xxxxxxxxxxxx" << endl;
    //cout << "Average Arrival Rate: " << avgArrivalRate << "     Average Service Time: " << avgServiceTime << endl;
    if (quantum != 0 && scheduleSelector == 4 ){
        cout << "Quantum: " << quantum << endl;
    }
    //avgServiceTime = .06;
    //avgArrivalRate = 1;
    //for(;avgArrivalRate < 31; avgArrivalRate++){
    initializeSimulation(avgArrivalRate);
    runSimulation(avgServiceTime, avgArrivalRate, scheduleSelector);
        //processToRun = 0;
        //systemIdleTime = 0;
        //averageProcessReadyQueue = 0;
        //numberTimesAvgProReadyQueueCalled = 0;
    //}

    return ;
}

int main(int argc, char*argv[])
{
    if(argc != 5){
        cout << "Usage: " << argv[0] << "<scheduler type> <average Arrival Rate> <average Service Time> <quantum> \n" << endl;
        return 1;
    }
    //seeding rand for uniform distribution
    srand((unsigned)time(NULL));
    //setting the averageServiceTime and averageArrivalRates that will be used
    //will be tested on .01 and .2
    quantum = stof(argv[4]);
    avgServiceTime = stof(argv[3]);
    avgArrivalRate = stof(argv[2]);
    scheduleSelector = stof(argv[1]);
    fullSimulation(scheduleSelector, avgArrivalRate, avgServiceTime, quantum);
    return 0;
}

