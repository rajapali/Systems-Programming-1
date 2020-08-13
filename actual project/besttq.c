#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>



/* CITS2002 Project 1 2019
   Name(s):             Deepali Rajawat
   Student number(s):   22496421
 */


//  besttq (v1.0)
//  Written by Chris.McDonald@uwa.edu.au, 2019, free for all to copy and modify

//  Compile with:  cc -std=c99 -Wall -Werror -o besttq besttq.c


//  THESE CONSTANTS DEFINE THE MAXIMUM SIZE OF TRACEFILE CONTENTS (AND HENCE
//  JOB-MIX) THAT YOUR PROGRAM NEEDS TO SUPPORT.  YOU'LL REQUIRE THESE
//  CONSTANTS WHEN DEFINING THE MAXIMUM SIZES OF ANY REQUIRED DATA STRUCTURES.

#define MAX_DEVICES             4
#define MAX_DEVICE_NAME         20
#define MAX_PROCESSES           50
#define MAX_EVENTS_PER_PROCESS  100

#define TIME_CONTEXT_SWITCH     5
#define TIME_ACQUIRE_BUS        5


//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

int optimal_time_quantum = 0;
int total_process_completion_time = 0;

//  ----------------------------------------------------------------------

#define CHAR_COMMENT            '#'
#define MAXWORD                 20

// THE FOLLOWING BELOW ARE ALL THE DATA STRUCTURES USED FOR THIS PROJECT

struct Processes {

    char name[MAXWORD];
    int startTime;
    int eventamt;
    int processid;
    int totaltimeSpent;
};

struct Events {

    char type[MAXWORD];
    int execTime;
    char deviceName[MAX_DEVICE_NAME];
    int transferamt;

};

struct Devices {

    char deviceName[MAX_DEVICE_NAME];
    int transferRate;

};

struct BQueue {

    int process[MAX_PROCESSES];
    int size;

};

// ----------------------------------------------------------------------

//INITIALISATION OF DATA STRUCTURES

struct Processes process[MAX_PROCESSES];
struct Events event[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
struct Devices device[MAX_DEVICES];
struct BQueue blockedQueue[MAX_DEVICES];
int readyQ[MAX_PROCESSES];
int currentevent[MAX_PROCESSES];                                //Current event each process is up to
int bestTimes[200][2];                                          //Array holding the completion times for each time quantum
int running;                                                    //Running state


int devSize = 0;                                                //Number of devices
int processNo = 0;
int eventNo = 0;
int size = 0;                                                   //Size of ready queue
int bestSize;                                                   //Size of bestTimes array
int currentIndex = 0;                                           //The current iteration simulation_job_mix is on
bool isOccupied = 0;                                            //states if CPU is occupied or not
int finishedProcesses = 0;                                      //Number of processes finished executing
int sysTime = 0;
int delayIncrement = 0;                                         //Used to make a context switch delay
int busDelay = 0;                                               //Used to make a data bus delay
bool busOccupied = 0;
double time = 0;                                               //The time taken to transfer data
int reqDevice = 0;                                             //The device that has been requested for data transfer

// -----------------------------------------------------------------------

// Reads the tracefile and stores its information into data structures.

void parse_tracefile(char program[], char tracefile[]) {

//  ATTEMPT TO OPEN OUR TRACEFILE, REPORTING AN ERROR IF WE CAN'T
    FILE *fp = fopen(tracefile, "r");

    if (fp == NULL) {
        printf("%s: unable to open '%s'\n", program, tracefile);
        exit(EXIT_FAILURE);
    }

    char line[BUFSIZ];
    int lc = 0;

//  READ EACH LINE FROM THE TRACEFILE, UNTIL WE REACH THE END-OF-FILE
    while (fgets(line, sizeof line, fp) !=
           NULL) {
        ++lc;

//  COMMENT LINES ARE SIMPLY SKIPPED
        if (line[0] == CHAR_COMMENT) {
            continue;
        }

//  ATTEMPT TO BREAK EACH LINE INTO A NUMBER OF WORDS, USING sscanf()
        char word0[MAXWORD], word1[MAXWORD], word2[MAXWORD], word3[MAXWORD];
        int nwords = sscanf(line, "%s %s %s %s", word0, word1, word2, word3);

        printf("%i = %s", nwords, line);

//  WE WILL SIMPLY IGNORE ANY LINE WITHOUT ANY WORDS
        if (nwords <= 0) {
            continue;
        }

//  LOOK FOR LINES DEFINING DEVICES, PROCESSES, AND PROCESS EVENTS
        if (nwords == 4 && strcmp(word0, "device") == 0) {

            if (devSize == 0) {

                //IF THERE ARE NO DEVICES IN THE STRUCT, ADD THIS DEVICE NORMALLY

                strcpy(device[0].deviceName, word1);
                device[0].transferRate = atoi(word2);
                devSize++;

            } else {

                //ELSE ADD THE NEW DEVICE IN THIS STRUCTURE BASED OFF ITS PRIORITY (TRANSFER RATE)

                for (int i = 0; i < devSize; i++) {

                    if (atoi(word2) >= device[i].transferRate) {

                        for (int j = devSize - 1; j >= i; j--) {

                            //MOVE ELEMENTS RIGHT TO MAKE SPACE FOR NEW DEVICE

                            strcpy(device[j + 1].deviceName, device[j].deviceName);
                            device[j + 1].transferRate = device[j].transferRate;
                        }

                        //ADD DEVICE INTO CORRECT INDEX

                        strcpy(device[i].deviceName, word1);
                        device[i].transferRate = atoi(word2);
                        devSize++;
                        break;
                    }
                }
            }
        } else if (nwords == 1 && strcmp(word0, "reboot") == 0) {

            ;   // NOTHING REALLY REQUIRED, DEVICE DEFINITIONS HAVE FINISHED

        } else if (nwords == 4 && strcmp(word0, "process") == 0) {

            //STORES PROCESS INFORMATION INTO PROCESS STRUCT

            strcpy(process[processNo].name, word1);
            process[processNo].startTime = atoi(word2);
            process[processNo].processid = processNo;
            process[processNo].totaltimeSpent = 0;



        } else if (nwords == 4 && strcmp(word0, "i/o") == 0) {

            //STORES I/O EVENT INFORMATION INTO EVENT STRUCT

            strcpy(event[processNo][eventNo].type, word0);
            event[processNo][eventNo].execTime = atoi(word1);
            strcpy(event[processNo][eventNo].deviceName, word2);
            event[processNo][eventNo].transferamt = atoi(word3);

            eventNo++;

        } else if (nwords == 2 && strcmp(word0, "exit") == 0) {

            //STORES EXIT EVENT INFORMATION INTO EVENT STRUCT

            strcpy(event[processNo][eventNo].type, word0);
            event[processNo][eventNo].execTime = atoi(word1);

            eventNo++;

        } else if (nwords == 1 && strcmp(word0, "}") == 0) {

            //  JUST THE END OF THE CURRENT PROCESS'S EVENTS

            process[processNo].eventamt = eventNo;
            processNo++;
            eventNo = 0;

        } else {

            printf("%s: line %i of '%s' is unrecognized",
                   program, lc, tracefile);
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);
}
// ------------------------------------------------------------------------------------

#undef  MAXWORD
#undef  CHAR_COMMENT

//START OF HELPER FUNCTIONS


//Given an array and its size, returns and pops the head of the queue and shifts all elements left

int pop(int a[], int size) {

    int first = a[0];

    for (int i = 0; i < size; i++) {

        a[i] = a[i + 1];    //shifts elements down

    }
    return first;
}


//Initialises variables to zero so that they can be used repetitively

void intitialise(void) {

    for (int q = 0; q < MAX_DEVICES; q++) {

        blockedQueue[q].size = 0;

    }

    for (int i = 0; i < processNo; i++) {

        process[i].totaltimeSpent = 0;
        currentevent[i] = 0;

    }

    sysTime = 0;
    finishedProcesses = 0;

}


// Checks if the event type of process i matches the input a

int eventType(int i, char a[]){

    return strcmp(event[i][currentevent[i]].type, a) == 0;

}


//Checks if the current event of process i has just finished executing

int hasExecuted(int i){

    int j = currentevent[i];    //the current event process i is on

    return process[i].totaltimeSpent == event[i][j].execTime;
}


//Checks if the time quantum has expired

int expireTQ(int a, int tq){

    return process[a].totaltimeSpent % tq == 0;

}


//Finds the device that the current event of process a is requesting and returns its index

int findDevice(int a){

    int b = currentevent[a];

    for(int k = 0; k < devSize; k++){

        if(strcmp(event[a][b].deviceName,device[k].deviceName) == 0){

            return k;

        }
    }

    return -1;
}


//Delays the switch of a process from running to ready

void contextSwitch(void){

    if (size != 0 && !isOccupied) {           //If the ready queue is not empty and the CPU is not occupied
        if (delayIncrement != TIME_CONTEXT_SWITCH) {

            delayIncrement++;

        } else {

            running = pop(readyQ, size);    //READY --> RUNNING
            --size;
            isOccupied = 1;
            delayIncrement = 0;

            printf("SYSTIME = %i READY %i--> RUNNING %i\n", sysTime, running, running);

        }

    }
}


//Calculates the transfer time of a process that has requested i/o

int calcTime(int dev, int proc){

    double data = event[proc][currentevent[proc]].transferamt;
    double rate = device[dev].transferRate/1000000.0;

    time = data/rate;

    double transferTime = time - (int) time;    //Gives remainder of the time calculated

    if (transferTime != 0.0) {                 //If time is not an integer, round up
        time++;
    }

    time = (int) time;                        //cast to int
    return time;

}


//Adds the current time quantum and its total completion time to an array

void addTimes(int k[][2], int completionTime, int TQ){

    bestTimes[currentIndex][0] = completionTime;
    bestTimes[currentIndex][1] = TQ;
    currentIndex++;

}

//Given an array of completion times and time quantums, finds the best time quantum that will produce the lowest completion time

void findBestTQ(int k[][2], int len){

    total_process_completion_time = k[0][0];
    optimal_time_quantum = k[0][1];

    for (int a = 0; a < len; a++) {

        if (k[a][0] <= total_process_completion_time) {     //If the current completion time is less than the one previously specified

            total_process_completion_time = k[a][0];        //make the completion time the current one
            optimal_time_quantum = k[a][1];                 //make the optimal time quantum the current one

        }
    }
}
//  -------------------------------------------------------------------------------------

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM

void simulate_job_mix(int time_quantum) {

    printf("running simulate_job_mix( time_quantum = %i usecs )\n",
           time_quantum);

    intitialise();                                              // Initialise values needed to zero

    while (finishedProcesses != processNo) {                    // While processes are still executing

        //Start admitting new processes to ready queue

        for (int j = 0; j < processNo; j++) {

            if (process[j].startTime == sysTime) {              //If the system time equals a process' commencement time

                readyQ[size] = process[j].processid;            // NEW --> READY
                ++size;

                printf("SYSTIME = %i NEW --> READY %i\n", sysTime, running);

            }

        }

        //Check if the event running on the CPU is an exit type

        if (isOccupied) {
            if (eventType(running, "exit")) {
                if (hasExecuted(running)) {                     //If the event has reached its exit time

                    isOccupied = 0;                             //Exit the CPU
                    finishedProcesses++;

                    printf("SYSTIME = %i RUNNING %i --> EXIT %i\n", sysTime, running, running);
                }

            }
        }

        //If the process running has been executing for the given time quantum

        if (expireTQ(running, time_quantum) && isOccupied && !hasExecuted(running)) {

            printf("SYSTIME = %i  TQ.EXPIRE  TT = %i\n", sysTime, process[running].totaltimeSpent);

            if (size != 0) {                                          //If there is a process waiting in the ready queue

                readyQ[size] = process[running].processid;            //RUNNING --> READY
                size++;
                isOccupied = 0;

                printf(" SYSTIME = %i RUNNING %i --> READY %i\n", sysTime, running, running);


            }

        }

        //If the CPU is unoccupied, perform a time context switch and then go from ready to running

        contextSwitch();

        //Check if the event running on the CPU is an i/o type

        if (isOccupied) {
            if (eventType( running, "i/o")) {

                if (hasExecuted(running)) {                 //If the event has reached its execution time
                      int k = findDevice(running);          //find which device it is requesting i/o for

                      if(k != -1){                          //If the device has been found

                            int bqsize = blockedQueue[k].size;

                            blockedQueue[k].process[bqsize] = running;          //RUNNING --> BLOCKED
                            blockedQueue[k].size++;

                            printf("SYSTIME = %i RUNNING %i --> BLOCKED %i\n", sysTime, running, running);
                            printf("EVENT %i\n", currentevent[running]);

                            isOccupied = 0;
                            contextSwitch();                         //Start context switch if ready queue is not empty


                      } else {

                          fprintf(stderr, "Error: Device not found!\n");
                          exit(EXIT_FAILURE);

                      }
                }
            }

            //If there is still a process running on the CPU, increment the accumulative time it has been executing for

            if (isOccupied) {
                process[running].totaltimeSpent++;
            }
        }

        /* If the blocked queue is not empty, find the transfer time of the process with the highest priority
         * and request the data bus */

        for(int h =0; h < devSize; h++){
            if(blockedQueue[h].size != 0 && !busOccupied){

                calcTime(h,blockedQueue[h].process[0]);
                busOccupied = 1;
                reqDevice = h;
                break;

            }
        }

        //If the data bus has been requested, start acquiring the bus and transferring the data

        if(busOccupied){

            if( busDelay != TIME_ACQUIRE_BUS + time){
            busDelay++;

        } else{

                int blockProc = blockedQueue[reqDevice].process[0];

                printf("TRANSFERTIME = %f\n", time);
                printf("SYSTIME = %i BLOCKED %i --> READY \n", sysTime, blockProc);

                //data has been transferred so the event has completed execution

                currentevent[blockProc]++;
                readyQ[size] = pop(blockedQueue[reqDevice].process, blockedQueue[reqDevice].size);   //BLOCKED --> READY
                size++;
                blockedQueue[reqDevice].size--;
                busDelay = 0;
                contextSwitch();                                 //Begin context switch if CPU is unoccupied
                busOccupied = 0;

                //Immediately start looking for the next process in the blocked queue to request the data bus

                for(int h = 0; h < devSize; h++) {
                    if (blockedQueue[h].size != 0) {

                        calcTime(h,blockedQueue[h].process[0]);
                        busOccupied = 1;
                        reqDevice = h;
                        busDelay++;
                        break;

                    }
                }

            }
        }

        //If all processes have not finished executing, increment the system time

        if (finishedProcesses != processNo) {
            sysTime++;
        }

    }

    int compTime = sysTime - process[0].startTime;          //total process completion time
    addTimes(bestTimes, compTime , time_quantum);           //add current TQ and completion time to array

    if (currentIndex == bestSize) {

        findBestTQ(bestTimes,bestSize);                     //Once all iterations have completed, find the best TQ

    }
}

//  ----------------------------------------------------------------------------

void usage(char program[]) {
    printf("Usage: %s tracefile TQ-first [TQ-final TQ-increment]\n", program);
    exit(EXIT_FAILURE);
}

int main(int argcount, char *argvalue[]) {
    int TQ0 = 0, TQfinal = 0, TQinc = 0;

//  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND THREE TIME VALUES
    if (argcount == 5) {
        TQ0 = atoi(argvalue[2]);
        TQfinal = atoi(argvalue[3]);
        TQinc = atoi(argvalue[4]);

        bestSize = ((TQfinal - TQ0) / TQinc) + 1;

        if (TQ0 < 1 || TQfinal < TQ0 || TQinc < 1) {
            usage(argvalue[0]);
        }
    }
//  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND ONE TIME VALUE
    else if (argcount == 3) {

        TQ0 = atoi(argvalue[2]);
        if (TQ0 < 1) {
            usage(argvalue[0]);
        }
        TQfinal = TQ0;
        TQinc = 1;
    }
//  CALLED INCORRECTLY, REPORT THE ERROR AND TERMINATE
    else {
        usage(argvalue[0]);
    }

//  READ THE JOB-MIX FROM THE TRACEFILE, STORING INFORMATION IN DATA-STRUCTURES
    parse_tracefile(argvalue[0], argvalue[1]);

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, VARYING THE TIME-QUANTUM EACH TIME.
//  WE NEED TO FIND THE BEST (SHORTEST) TOTAL-PROCESS-COMPLETION-TIME
//  ACROSS EACH OF THE TIME-QUANTA BEING CONSIDERED

    for (int time_quantum = TQ0; time_quantum <= TQfinal; time_quantum += TQinc) {
        simulate_job_mix(time_quantum);

    }

    // PRINT ALL COMPLETION TIMES AND TIME QUANTUMS
    for (int i = 0; i < bestSize; i++) {
        printf("%i  %i\n", bestTimes[i][0], bestTimes[i][1]);
    }

//  PRINT THE PROGRAM'S RESULT
    printf("best %i %i\n", optimal_time_quantum, total_process_completion_time);

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4
