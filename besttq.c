#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "queue.h"

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
int total_process_completion_time       = INT_MAX;

//  ----------------------------------------------------------------------
// DATA STRUCTURES 

// DEVICES
int  device_num = 0; // Number of devices
char device_name[MAX_DEVICES][MAX_DEVICE_NAME + 1] = {""};
int  device_rate[MAX_DEVICES] = {INT_MAX};

// may need a queue for each device to store upcoming i/o

// PROCESSES
#define EVENT_IO 1
#define EVENT_EXIT 2

int proc_num = 0;
int proc_start_time[MAX_PROCESSES];
int proc_total_events[MAX_PROCESSES];
int proc_event_type[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS]; // i/o or exit
int proc_event_time[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
int proc_event_device[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
int proc_event_data[MAX_PROCESSES][MAX_EVENTS_PER_PROCESS];
char proc_started[MAX_PROCESSES];
int proc_current_event[MAX_PROCESSES];
int proc_total_time[MAX_PROCESSES]; // Total time proc has run for


// Simulation data
#define NO_EVENT -1
#define PROC_EVENT 1     //next event is a processor event - TQ, i/o req, exit
#define IO_FINISH 2   // next event is an i/o event completing
#define NEW_PROC 3 // a new process needs to be added to ready queue

#define NO_PROC -1

int system_time = 0;

struct queue *sim_ready_queue;
struct queue *sim_device_queue[MAX_DEVICES];


//  ----------------------------------------------------------------------
//  QUEUE HELPER FUNCTIONS


//  ----------------------------------------------------------------------

#define CHAR_COMMENT            '#'
#define MAXWORD                 20

int device_id(char name[])
{
    for (int i = 0; i < device_num; i++) {
        if (strcmp(device_name[i], name) == 0) {
            return i;
	}
    }
    fprintf(stderr, "Could not find device: %s", name);
    exit(EXIT_FAILURE);
}


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
            proc_event_device[proc_num][proc_total_events[proc_num]] = device_id(word2);
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

// Finds id of next process that will start
int next_proc() {
    int first_id = NO_PROC;
    int first_time = INT_MAX;
    
    for (int i = 0; i < proc_num; i++) {
        if (proc_started[i] == 0 && proc_start_time[i] < first_time) {
            first_id = i;
            first_time = proc_start_time[i];
        }
    }
    
    if (first_id != NO_PROC) {
        proc_started[first_id] = 1;
    }
    
    return first_id;
}

int find_event_time(int time_quantum, int context_switch_time, int proc_id) {
    int process_time_to_next_event = proc_event_time[proc_id][proc_current_event[proc_id]] - proc_total_time[proc_id];
    
    if (process_time_to_next_event <= time_quantum) {
        proc_total_time[proc_id] += process_time_to_next_event;
        return system_time + context_switch_time + process_time_to_next_event;
    } else {
        proc_total_time[proc_id] += time_quantum;
        return system_time + context_switch_time + time_quantum;
    }
}

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
void simulate_job_mix(int time_quantum)
{
    // Initalise queues
    sim_ready_queue = new_queue(MAX_PROCESSES);

    for (int i = 0; i < device_num; i++)
    {
        sim_device_queue[i] = new_queue(MAX_PROCESSES);
    }

    // Reset state

    int sim_curr_run = NO_PROC;
    int sim_curr_run_event_time = 0;
    int sim_curr_io = NO_PROC;
    int sim_curr_io_event_time = 0;
    int sim_next_start = NO_PROC;
    int sim_next_start_time = 0;

    system_time = 0;
    int first_start_time = 0;
    
    for (int i = 0; i < proc_num; i++)
    {
        proc_started[i] = 0;
        proc_current_event[i] = 0;
        proc_total_time[i] = 0;
        
    }
    
    // initalise event
    
    sim_next_start = next_proc();
    sim_next_start_time = proc_start_time[sim_next_start];
    first_start_time = sim_next_start_time;

    printf("First proc id: %i at time %ius\n\n", sim_next_start, sim_next_start_time);
    
    printf("Starting simulation with TQ: %4ius\n", time_quantum);
    
    // Do some sim
    char running = 1;

    while (running != 0) {
        int next_event_type = NO_EVENT;
        int next_event_time = INT_MAX;
        
        if (sim_curr_run != NO_PROC && sim_curr_run_event_time < next_event_time) {
            next_event_type = PROC_EVENT;
            next_event_time = sim_curr_run_event_time;
        }
        
        if (sim_curr_io != NO_PROC && sim_curr_io_event_time < next_event_time) {
            next_event_type = IO_FINISH;
            next_event_time = sim_curr_io_event_time;
        }
        
        if (sim_next_start != NO_PROC && sim_next_start_time < next_event_time) {
            next_event_type = NEW_PROC;
            next_event_time = sim_next_start_time;
        }
        
        switch (next_event_type) {
            case PROC_EVENT:
                printf("EVENT | type: PROC_EVENT time: %6ius\n", next_event_time);
                printf("      | Advancing system time from %6ius to %6ius\n", system_time, next_event_time);
                printf("      | Current process time (%i) %ius\n", sim_curr_run, proc_total_time[sim_curr_run]);
                printf("      | Current next event time (%i, %i) %ius\n", sim_curr_run, proc_current_event[sim_curr_run], proc_event_time[sim_curr_run][proc_current_event[sim_curr_run]]);
                system_time = next_event_time;
                
                // i/o, TQ or exit?
                
                if (proc_total_time[sim_curr_run] == proc_event_time[sim_curr_run][proc_current_event[sim_curr_run]]) {
                    proc_current_event[sim_curr_run]++;
                    // i/o or exit
                    
                    // exit
                    printf("EXIT  | Exit event occured for process %i\n", sim_curr_run);
                    if (is_empty(sim_ready_queue)) {
                        sim_curr_run = NO_PROC;
                    } else {
                        sim_curr_run = front(sim_ready_queue);
                        dequeue(sim_ready_queue);
                        
                        sim_curr_run_event_time = find_event_time(time_quantum, TIME_CONTEXT_SWITCH, sim_curr_run);
                    }
                } else {
                    printf("      | Timeout occured\n");
                    // TQ
                    if (is_empty(sim_ready_queue)) {
                        printf("      | No process ready. Continue running. (%i) -> (%i)\n", sim_curr_run, sim_curr_run);
                        // cont. running no context switch
                        
                        sim_curr_run_event_time = find_event_time(time_quantum, 0, sim_curr_run);
                    } else {
                        printf("      | Process ready. Context swtich. (%i) -> (%i)\n", sim_curr_run, front(sim_ready_queue));
                        enqueue(sim_ready_queue, sim_curr_run);
                        sim_curr_run = front(sim_ready_queue);
                        dequeue(sim_ready_queue);
                        
                        sim_curr_run_event_time = find_event_time(time_quantum, TIME_CONTEXT_SWITCH, sim_curr_run);
                    }
                }
                
                break;
                
            case IO_FINISH:
                printf("EVENT | type: IO_FINISH  time: %4ius\n", next_event_time);
                break;
            
            case NEW_PROC:
                printf("EVENT | type: NEW_PROC   time: %6ius\n", next_event_time);
                printf("      | Advancing system time from %6ius to %6ius\n", system_time, next_event_time);
                system_time = next_event_time;
                
                if (sim_curr_run == NO_PROC) {
                    sim_curr_run = sim_next_start;
                    
                    sim_curr_run_event_time = find_event_time(time_quantum, TIME_CONTEXT_SWITCH, sim_curr_run);
                } else {
                    enqueue(sim_ready_queue, sim_next_start);
                }
                
                sim_next_start = next_proc();
                if (sim_next_start != NO_PROC) {
                    sim_next_start_time = proc_start_time[sim_next_start];
                }
                
                break;
                
            case NO_EVENT:
                printf("EVENT | type: NO_EVENT   time: %4ius\n", system_time);
                running = 0;
                // FINSIH HERE
                break;
            default:
                fprintf(stderr, "Bruh.jpg\n");
                exit(EXIT_FAILURE);
                break;
        }
    }
    
    printf("Finished...\n");
    printf("\033[1;31m");
    printf("Runtime %ius\n\n", system_time - first_start_time);
    printf("\033[0m");

    if (system_time - first_start_time <= total_process_completion_time) {
        total_process_completion_time = system_time - first_start_time;
        optimal_time_quantum = time_quantum;
    }


    // Free queue
    free_queue(sim_ready_queue);
    sim_ready_queue = NULL;
    
    for (int i = 0; i < device_num; i++)
    {
        free_queue(sim_device_queue[i]);
        sim_device_queue[i] = NULL;
    }
    
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
