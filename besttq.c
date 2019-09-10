#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* CITS2002 Project 1 2019
   Name(s):             Alexander Shearer, Thomas Kinsella
   Student number(s):   22465777, 22177293
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
// DO NOT USE THIS - #define MAX_PROCESS_EVENTS      1000
#define MAX_EVENTS_PER_PROCESS	100

#define TIME_CONTEXT_SWITCH     5
#define TIME_ACQUIRE_BUS        5


//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

int optimal_time_quantum                = 0;
int total_process_completion_time       = 0;

//  ----------------------------------------------------------------------
// DATA STRUCTURES 

// DEVICES
int  device_num = 0; // Number of devices
char device_name[MAX_DEVICES][MAX_DEVICE_NAME + 1] = {""};
int  device_rate[MAX_DEVICES] = {INT_MAX};

// may need a queue for each device to store upcoming i/o

// PROCESSES
#define EVENT_IO 2
#define EVENT_EXIT 1

int proc_num = 0;
int proc_start_time[MAX_PROCESSES]    = {0};
int proc_total_events[MAX_PROCESSES]  = {0};
int proc_current_event[MAX_PROCESSES] = {0}; // Stores which event the proc will do next
int proc_event_type[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS]   = {0}; // i/o or exit
int proc_event_time[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS]   = {0};
int proc_event_device[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS] = {0};
int proc_event_data[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS]   = {0};


//  ----------------------------------------------------------------------


#define CHAR_COMMENT            '#'
#define MAXWORD                 20

void parse_tracefile(char program[], char tracefile[])
{
    //  ATTEMPT TO OPEN OUR TRACEFILE, REPORTING AN ERROR IF WE CAN'T
    FILE *fp    = fopen(tracefile, "r");

    if(fp == NULL) {
        printf("%s: unable to open '%s'\n", program, tracefile);
        exit(EXIT_FAILURE);
    }

    char line[BUFSIZ];
    int  lc     = 0;

    //  READ EACH LINE FROM THE TRACEFILE, UNTIL WE REACH THE END-OF-FILE
    while(fgets(line, sizeof line, fp) != NULL) {
        ++lc;

        //  COMMENT LINES ARE SIMPLY SKIPPED
        if(line[0] == CHAR_COMMENT) {
            continue;
        }

        // ATTEMPT TO BREAK EACH LINE INTO A NUMBER OF WORDS, USING sscanf()
        char    word0[MAXWORD], word1[MAXWORD], word2[MAXWORD], word3[MAXWORD];
        int nwords = sscanf(line, "%s %s %s %s", word0, word1, word2, word3);

        //      printf("%i = %s", nwords, line);

        //  WE WILL SIMPLY IGNORE ANY LINE WITHOUT ANY WORDS
        if(nwords <= 0) {
            continue;
        }

        //  LOOK FOR LINES DEFINING DEVICES, PROCESSES, AND PROCESS EVENTS

        if(nwords == 4 && strcmp(word0, "device") == 0) {
            // Device found
            strcpy(device_name[device_num], word1);
            device_rate[device_num] = atoi(word2);
            device_num++;
        }

        else if(nwords == 1 && strcmp(word0, "reboot") == 0) {
            ;   // NOTHING REALLY REQUIRED, DEVICE DEFINITIONS HAVE FINISHED
        }

        else if(nwords == 4 && strcmp(word0, "process") == 0) {
            proc_start_time[proc_num] = atoi(word2);
        }

        else if(nwords == 4 && strcmp(word0, "i/o") == 0) {
            proc_event_type[proc_num][proc_total_events[proc_num]] = EVENT_IO;
            proc_event_time[proc_num][proc_total_events[proc_num]] = atoi(word1);
            proc_event_device[proc_num][proc_total_events[proc_num]] = atoi(word2);
            proc_event_data[proc_num][proc_total_events[proc_num]] = atoi(word3);
            proc_total_events[proc_num]++;
        }

        else if(nwords == 2 && strcmp(word0, "exit") == 0) {
            proc_event_type[proc_num][proc_total_events[proc_num]] = EVENT_EXIT;
            proc_event_time[proc_num][proc_total_events[proc_num]] = atoi(word1);
            proc_total_events[proc_num]++;

            proc_num++; // End of this process
        }

        else if(nwords == 1 && strcmp(word0, "}") == 0) {
            ;   //  JUST THE END OF THE CURRENT PROCESS'S EVENTS
        }
        else {
            printf("%s: line %i of '%s' is unrecognized",
                        program, lc, tracefile);
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);


    // DEBUG

    // Print devices & rates
    printf("-- Devices --\nNumber of devices: %i\n", device_num);

    for (int i = 0; i < device_num; i++)
    {
        printf("%6s: %10i bytes/sec\n", device_name[i], device_rate[i]);
    }

    printf("\n");

    // Print processes
    printf("-- Processes --\nNumber of processes: %i\n", proc_num);

    for (int i = 0; i < proc_num; i++)
    {
        printf("Process: %2i | Start time: %ius | Number of events: %3i\n", i, proc_start_time[i], proc_total_events[i]);
        for (int j = 0; j < proc_total_events[i]; j++)
        {
            if (proc_event_type[i][j] == EVENT_IO)
            {
                printf("\tEvent type:  i/o | Occurrence time: %5i | Device name: %6s | Data: %6i bytes\n", proc_event_time[i][j], device_name[proc_event_device[i][j]], proc_event_data[i][j]);
            } else {
                printf("\tEvent type: exit | Occurrence time: %5i\n", proc_event_time[i][j]);
            }
            
            
        }
        
    }
    
    printf("\n\n");
    
}

#undef  MAXWORD
#undef  CHAR_COMMENT

//  ----------------------------------------------------------------------

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
void simulate_job_mix(int time_quantum)
{
    printf("running simulate_job_mix( time_quantum = %i usecs )\n",
                time_quantum);
}

//  ----------------------------------------------------------------------

void usage(char program[])
{
    printf("Usage: %s tracefile TQ-first [TQ-final TQ-increment]\n", program);
    exit(EXIT_FAILURE);
}

int main(int argcount, char *argvalue[])
{
    int TQ0 = 0, TQfinal = 0, TQinc = 0;

//  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND THREE TIME VALUES
    if(argcount == 5) {
        TQ0     = atoi(argvalue[2]);
        TQfinal = atoi(argvalue[3]);
        TQinc   = atoi(argvalue[4]);

        if(TQ0 < 1 || TQfinal < TQ0 || TQinc < 1) {
            usage(argvalue[0]);
        }
    }
//  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND ONE TIME VALUE
    else if(argcount == 3) {
        TQ0     = atoi(argvalue[2]);
        if(TQ0 < 1) {
            usage(argvalue[0]);
        }
        TQfinal = TQ0;
        TQinc   = 1;
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

    for(int time_quantum=TQ0 ; time_quantum<=TQfinal ; time_quantum += TQinc) {
        simulate_job_mix(time_quantum);
    }

//  PRINT THE PROGRAM'S RESULT
    printf("best %i %i\n", optimal_time_quantum, total_process_completion_time);

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4
