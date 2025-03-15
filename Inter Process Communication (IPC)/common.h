#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>

#define MAX_CONFIG_LINES 100
#define MAX_LINE_SIZE 1000
#define MAX_CHILDREN 100
#define MAX_REQUESTS 10

 /*
 * structure for the shared memory used for inter-process communication (IPC) between
 * the parent and child processes. The structure contains:
 *-activeChildIndx:index of the currently active child process, or -1 if none
 *-endsInTimestamp:the termination timestamp for the most recently terminated child
 *-sharedSpace:buffer used to exchange a single line of text between parent and child
 * */
typedef struct SharedMemory {
    int activeChildIndx;        
    int endsInTimestamp;           
    char sharedSpace[MAX_LINE_SIZE]; 
} SharedMemory;


int freeSlotFinder(pid_t *children, int M);
int childLabelFinder(char child_labels[][10], const char *label, int M);
int lineCount(FILE *fp);
int randomActiveChildrenSelection(pid_t *children, int M);
void randomLineSelection(FILE *fp, char *buffer, int total_lines);
SharedMemory *sharedMemSetup();
void rescourceCleanup(SharedMemory *sharedMem, sem_t *semaphores[], int M, sem_t *parentNotificationSemaphore);
void semInit(sem_t *semaphores[], int M, sem_t **parentNotificationSemaphore);

