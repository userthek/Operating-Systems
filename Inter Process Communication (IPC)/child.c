#include "child.h"
#include <time.h>

/**
 * createChildFile
 * #################################################################################################
 * intended to create or open a `.log` file for the child process
 * the file is uniquely named after the child's PID to ensure each child has its own log file
 *
 * ->Parameters
 * --------------------------------
 *  -PID, used to name the log file
 *
 * ->Returns
 * ----------------------------------------------
 *  -file ptr to the opened/created log file
 *  - and if the file cannot be created or opened, the program terminates with an error
 */
static FILE* createChildFile(int PID) {
    char fileName[20]; //used to allocate enough space for the filename (e.g., "file[12345].log"), 
					   //20might seem a bit too much, but it is purely for safety purposes
    snprintf(fileName, sizeof(fileName), "file[%d].log", PID); 
    return fopen(fileName, "a"); 
}

/**
 * #############################################################################################
 * Implements the functionality of a child process in the parent-child communication system
 *
 * The child process:
 * -waits for the parent process to send messages using a semaphore specific to the child
 * -on receiving a message:
 *   -if the message is "TERMINATE":
 *       -logs the termination message received from the parent
 *       - calculates its active time based on the current timestamp stored in shared memory
 *       -logs the total number of lines received during its lifetime
 *       -notifies the parent process using the parentNotificationSemaphore and exits
 *   -for any other message:
 *       -logs the received message to its unique `.log` file
 *       -increments the counter for total lines received
 *       -notifies the parent process using the parentNotificationSemaphore
 * -and the process continues to wait for the next message in a blocked state until terminated
 *
 * Parameters:
 *  -seg_semaphores:array of semaphores used for parent-child communication
 *  -M_requests:the index of the semaphore assigned to this child
 *  -sharedMem:pointer to the shared memory structure used for inter-process communication
 *  -activation_time:timestamp when the child process was spawned
 *  -parentNotificationSemaphore:semaphore used to notify the parent process after processing a message
 */



void child(sem_t *seg_semaphores[], int M_requests, SharedMemory *sharedMem, int activation_time, sem_t *parentNotificationSemaphore) {
    int lineCntr = 0;
    int endsInTimestamp = -1;
    FILE *writefile = createChildFile(getpid());
    if (!writefile) {
        perror("Could not open .log file");
        exit(EXIT_FAILURE);
    }

    while (1) {
        if (sem_wait(seg_semaphores[M_requests]) < 0) {
            perror("Fail performing sem_wait()");
            exit(EXIT_FAILURE);
        }

        if (strcmp(sharedMem->sharedSpace, "TERMINATE") == 0) {
            fprintf(writefile, "[t = %d] Child[%d] received TERMINATE message. Exiting.\n", sharedMem->endsInTimestamp, getpid());//writes in the.log files the message recieved
            printf("[t = %d] Child[%d] received TERMINATE message. Exiting.\n", sharedMem->endsInTimestamp, getpid());//writes in the console the termination  message
            endsInTimestamp = sharedMem->endsInTimestamp;
            sem_post(parentNotificationSemaphore);
            break;
        }

        fprintf(writefile, "[t = %d] Child[%d] received message: %s", sharedMem->endsInTimestamp, getpid(), sharedMem->sharedSpace);//one for the .log file
        printf("[t = %d] Child[%d] received message: %s", sharedMem->endsInTimestamp, getpid(), sharedMem->sharedSpace);///and another message for the console
        lineCntr++;
        sem_post(parentNotificationSemaphore);
    }

    if (endsInTimestamp >= 0) {
        int timeActive = endsInTimestamp - activation_time;
        fprintf(writefile, "Child[%d] terminated. Total lines received: %d, Active time: %d - %d = %d steps\n",
                getpid(), lineCntr, endsInTimestamp, activation_time, timeActive);
    }

    fclose(writefile);
    exit(EXIT_SUCCESS);
}


