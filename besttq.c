#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

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

#define STATE_NEW 1    // Process has not yet started
#define STATE_READY 2  // Process has started, may be ready, running, blocked or exited

struct Event {
    int type;
    int time;
    int device;
    int data;
};

struct Process {
    int id; // id as passed by tracefile, only used for human readable i/o
    int start_time;
    int total_events;
    struct Event events[MAX_EVENTS_PER_PROCESS];
    int state;
    int current_event;
    int total_time;    // total time a given process has run for
};

int proc_num = 0;
struct Process* processes[MAX_PROCESSES];

struct Event* current_event(struct Process* proc) {
    return &proc->events[proc->current_event];
}

// Simulation data
#define NO_EVENT -1
#define PROC_EVENT 1     //next event is a processor event - TQ, i/o req, exit
#define IO_FINISH 2   // next event is an i/o event completing
#define NEW_PROC 3 // a new process needs to be added to ready queue

#define NO_PROC -1
#define NO_DEVICE -1

int system_time = 0;

struct queue *sim_ready_queue;
struct queue *sim_device_queue[MAX_DEVICES];


// Flags
int debug_enable = 0; 


//  ----------------------------------------------------------------------
//  QUEUE HELPER FUNCTIONS


//  ----------------------------------------------------------------------

#define CHAR_COMMENT            '#'
#define MAXWORD                 20

int find_device_id(char name[])
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
            processes[proc_num]->start_time = atoi(word2);
            processes[proc_num]->id = atoi(word1);
            processes[proc_num]->state = STATE_NEW;
        }

        else if(nwords == 4 && strcmp(word0, "i/o") == 0) {
            struct Event *event = &processes[proc_num]->events[processes[proc_num]->total_events];
            event->type = EVENT_IO;
            event->time = atoi(word1);
            event->device = find_device_id(word2);
            event->data = atoi(word3);
            processes[proc_num]->total_events++;
        }

        else if(nwords == 2 && strcmp(word0, "exit") == 0) {
            struct Event *event = &processes[proc_num]->events[processes[proc_num]->total_events];
            event->type = EVENT_EXIT;
            event->time = atoi(word1);
            processes[proc_num]->total_events++;

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
    if (debug_enable) {
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
            struct Process *process = processes[i];
            printf("Process: %2i | Start time: %ius | Number of events: %3i\n", process->id, process->start_time, process->total_events);
            for (int j = 0; j < process->total_events; j++)
            {                
                struct Event *event = &process->events[j];
                if (event->type == EVENT_IO)
                {
                    printf("\tEvent type:  i/o | Occurrence time: %5i | Device name: %6s | Data: %6i bytes\n", event->time, device_name[event->device], event->data);
                } else {
                    printf("\tEvent type: exit | Occurrence time: %5i\n", event->time);
                }
            }
        }
        
        printf("\n\n");
    }
}

#undef  MAXWORD
#undef  CHAR_COMMENT

//  ----------------------------------------------------------------------

// Finds id of next process that will start
int next_proc() {
    int first_id = NO_PROC;
    int first_time = INT_MAX;
    
    for (int i = 0; i < proc_num; i++) {
        if (processes[i]->state == STATE_NEW && processes[i]->start_time < first_time) {
            first_id = i;
            first_time = processes[i]->start_time;
        }
    }
    
    if (first_id != NO_PROC) {
        processes[first_id]->state = STATE_READY;
    }

    return first_id;
}

int find_event_time(int time_quantum, int context_switch_time, int proc_id) {
    struct Process *process = processes[proc_id];

    int process_time_to_next_event = current_event(process)->time - process->total_time;
    
    if (process_time_to_next_event <= time_quantum) {
        process->total_time += process_time_to_next_event;
        return system_time + context_switch_time + process_time_to_next_event;
    } else {
        process->total_time += time_quantum;
        return system_time + context_switch_time + time_quantum;
    }
}

void enter_ready_queue(int process_id) {
    if (debug_enable) {
        printf("      | Process (%02i) entering ready queue\n", process_id);
    }

    enqueue(sim_ready_queue, process_id);
}

void enter_device_queue(int process_id, int device_id) {
    if (debug_enable) {
        printf("      | Process (%02i) entering '%s' device queue\n", process_id, device_name[current_event(processes[device_id])->device]);
    }

    enqueue(sim_device_queue[device_id], process_id);                    
}

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
void simulate_job_mix(int time_quantum)
{
    printf("Testing with TQ = %5ius... ", time_quantum);
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
        processes[i]->current_event = 0;
        processes[i]->total_time = 0;
        processes[i]->state = STATE_NEW;
    }
    
    // create first event
    sim_next_start = next_proc();
    sim_next_start_time = processes[sim_next_start]->start_time;

    first_start_time = sim_next_start_time;

    if (debug_enable){
        printf("First proc id: %i at time %ius\n\n", sim_next_start, sim_next_start_time);
        printf("Starting simulation with TQ: %4ius\n", time_quantum);
    }
    

    char running = 1;

    while (running != 0) {
        // Determine next event
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
                if (debug_enable) {
                    printf("EVENT | type: PROC_EVENT time: %6ius\n", next_event_time);
                    printf("      | Advancing system time from %6ius to %6ius\n", system_time, next_event_time);
                    printf("      | Current process time (%i) %ius\n", sim_curr_run, processes[sim_curr_run]->total_time);
                    printf("      | Current next event time (%i, %i) %ius\n", sim_curr_run, processes[sim_curr_run]->current_event, current_event(processes[sim_curr_run])->time);
                    printf("      | sim_curr_run = %i sim_curr_io = %i sim_next_start = %i\n", sim_curr_run, sim_curr_io, sim_next_start);
                }
                
                system_time = next_event_time;
                
                // Timeout or processor event reached?
                if (processes[sim_curr_run]->total_time == current_event(processes[sim_curr_run])->time) {
                    if (current_event(processes[sim_curr_run])->type == EVENT_IO) {
                        if (debug_enable) {
                            printf("      | (%i) I/O request event occured for process %i\n", processes[sim_curr_run]->current_event, sim_curr_run);
                        }
                        
                        enter_device_queue(sim_curr_run, current_event(processes[sim_curr_run])->device);
                    } else {                    
                        if (debug_enable) {
                            printf("EXIT  | Exit event occured for process %i\n", sim_curr_run);
                        }

                        processes[sim_curr_run]->current_event++; // Event processed, iterate
                    }
                    
                    // Process leaves cpu
                    sim_curr_run = NO_PROC;
                } else {
                    if (debug_enable) {
                        printf("      | Timeout occured\n");
                    }

                    if (is_empty(sim_ready_queue)) {
                        if (debug_enable) {
                            printf("      | No process ready. Process %i will continue running.\n", sim_curr_run);
                        }
                        
                        sim_curr_run_event_time = find_event_time(time_quantum, 0, sim_curr_run);
                    } else {
                        if (debug_enable) {
                            printf("      | Processes waiting\n");
                        }

                        enter_ready_queue(sim_curr_run);
                        sim_curr_run = NO_PROC;
                    }
                }
                
                break;
                
            case IO_FINISH:
                if (debug_enable) {
                    printf("EVENT | type: IO_FINISH  time: %4ius\n", next_event_time);
                    printf("      | Process ID: %i (%i)\n", sim_curr_io, sim_curr_run);
                }
                system_time = next_event_time;

                processes[sim_curr_io]->current_event++;

                enter_ready_queue(sim_curr_io);               
                sim_curr_io = NO_PROC;
                
                break;
            
            case NEW_PROC:
                if (debug_enable) {
                    printf("EVENT | type: NEW_PROC   time: %6ius\n", next_event_time);
                    printf("      | Advancing system time from %6ius to %6ius\n", system_time, next_event_time);
                }
                system_time = next_event_time;
                
                enter_ready_queue(sim_next_start);
                
                sim_next_start = next_proc();
                if (sim_next_start != NO_PROC) {
                    sim_next_start_time = processes[sim_next_start]->start_time;
                }
                
                break;
                
            case NO_EVENT:
                if (debug_enable) {
                    printf("END   | time: %4ius\n", system_time);
                }
                running = 0;
                break;
            default:
                fprintf(stderr, "Bruh.jpg\n");
                exit(EXIT_FAILURE);
                break;
        }

        // Check to move process from ready to running
        if (sim_curr_run == NO_PROC && !is_empty(sim_ready_queue))
        {
            sim_curr_run = sim_curr_io;

            sim_curr_run = front(sim_ready_queue);
            dequeue(sim_ready_queue);
            
            sim_curr_run_event_time = find_event_time(time_quantum, TIME_CONTEXT_SWITCH, sim_curr_run);

            if (debug_enable)
            {
                printf("CPU   | No process currently running\n");
                printf("      | Moving (%02i) to running\n", sim_curr_run);
                printf("      | Next processor event at %6ius\n", sim_curr_run_event_time);
            }
        }
        
        // Find fastest I/O with processes in queue and 'lock' databus
        if (sim_curr_io == NO_PROC)
        {
            int best_device_id = NO_DEVICE;
            int best_device_rate = 0;
            
            for (int i = 0; i < device_num; i++) {
                if (!is_empty(sim_device_queue[i])) {
                    if (device_rate[i] > best_device_rate) {
                        best_device_id = i;
                        best_device_rate = device_rate[i];
                    }
                }
            }
            
            if (best_device_id != NO_DEVICE) {
                int next_io_proc = front(sim_device_queue[best_device_id]);
                dequeue(sim_device_queue[best_device_id]);                

                // Put device
                double this_device_rate = (double) best_device_rate / 1000000.0; // second -> us
                double data_amount = current_event(processes[next_io_proc])->data;
                int transfer_time = (int) ceil( data_amount / this_device_rate);
                
                sim_curr_io = next_io_proc;
                sim_curr_io_event_time = system_time + transfer_time + TIME_ACQUIRE_BUS;
                
                if (debug_enable) {
                    printf("BUS   | Fastest device with i/o waiting is '%s'\n", device_name[best_device_id]);
                    printf("      | Process (%i) is engaging bus. Transfer time %ius. Complete time %ius\n", next_io_proc, transfer_time, sim_curr_io_event_time);
                }
            }
        }
    }
    
    printf("Finished | ");
    printf("\033[1;31m");
    printf("Runtime %10ius\n", system_time - first_start_time);
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

    // Allocate space for data structures
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        processes[i] = (struct Process*)malloc(sizeof(struct Process));
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
    printf("\nbest %i %i\n", optimal_time_quantum, total_process_completion_time);

    // Free malloced space
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        free(processes[i]);
        processes[i] = NULL;
    }

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4
