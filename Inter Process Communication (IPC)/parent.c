#include "common.h"
#include "child.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <semaphore.h>
#include <sys/wait.h>

/**
 * ConfigEntry
 * #################################################################################################
 * Represents a single entry in the configuration file, which defines actions the parent process
 * will execute at specified timestamps. Each entry consists of:
 * 
 * -> Fields
 * ----------
 * - timestamp: an integer representing the time step when the action should occur (e.g., 5, 10, 15).
 * - processLabel: a string (up to 9 characters + null terminator) identifying the target process
 *   (e.g., "C1" for child 0).
 * - command: a character specifying the action type. It can take the following values:
 *     * 'S': Spawn a new child process.
 *     * 'T': Terminate an existing child process.
 *     * 'E': Exit the simulation (used to define the end time of the simulation).
 */
typedef struct ConfigEntry {
    int timestamp;//first column
    char processLabel[10];//second column
    char command;//third column of the config.txt file
} ConfigEntry;

/**configInfo
 *#################################################################################################
 *parses the configuration file to extract commands and data for the parent process
 *reads the file line by line and populates an array of ConfigEntry structures
 *
 *->Parameters
 *------------
 *-filename:name of the configuration file
 *-lineCount:pointer to store the total number of lines in the file
 *
 *->Returns
 *---------
 *-dynamically allocated array of ConfigEntry structures
 *-the caller is responsible for freeing the memory after usage
 */
ConfigEntry *configInfo(const char *filename, int *lineCount) {
    FILE *fp = fopen(filename, "r");//open config file for reading
    if (!fp) {
        perror("Failed to open configuration file");
        exit(EXIT_FAILURE);
    }
    int count = 0;//initialization of the line counter
    char line[256];//buffer to read each line, I thought that 256 is a generous upper limit
    while (fgets(line, sizeof(line), fp)) {
        count++;//and now we are incrementing counter for each line read
    }
    rewind(fp);//resetting the file pointer to the beginning for parsing

	//we allocate memory for ConfigEntry array (one entry per line)
    ConfigEntry *commands = malloc(count * sizeof(ConfigEntry));
    if (!commands) {
        perror("Failed to allocate memory for commands");
        exit(EXIT_FAILURE);
    }

    int i = 0; 
    while (fgets(line, sizeof(line), fp)) {
		//we check if the command is EXIT
        if (strstr(line, "EXIT")) {
            int timestamp;
            if (sscanf(line, "%d EXIT", &timestamp) == 1) {
                commands[i].timestamp = timestamp;
                strcpy(commands[i].processLabel, "EXIT");
                commands[i].command = 'E';
            } else {
                fprintf(stderr, "Error parsing EXIT command in configuration file.\n");
                free(commands);
                exit(EXIT_FAILURE);
            }
        } else {
			//(SPAWN/TERMINATE)
            char processLabel[10];
            if (sscanf(line, "%d %s %c", &commands[i].timestamp,
                       processLabel, &commands[i].command) == 3) {
						// Store the parsed data in the commands array
                strcpy(commands[i].processLabel, processLabel);
            } else {
                fprintf(stderr, "Error parsing command in configuration file: %s\n", line);
                free(commands);
                exit(EXIT_FAILURE);
            }
        }
        i++;
    }
    *lineCount = count; //updating the line count for the caller
    fclose(fp);
    return commands;
}

/**
 * parentProcess
 * #################################################################################################
 * implements the core logic of the parent process in the parent-child communication system
 * the parent process:
 * -reads and interprets commands from the configuration file to manage child processes
 * -manages the lifecycle of child processes (spawn and terminate) according to configuration commands
 * -uses shared memory and semaphores for communication with child processes
 * -sends random lines from a text file to active child processes
 * -terminates all active child processes upon encountering the EXIT command in the configuration file
 *
 * detailed functionality:
 * --------------------------
 * 1. initialization:
 *    -reads the configuration file (config_file) and loads all commands into memory
 *    -opens the text file (text_file) to retrieve random lines for communication with child processes
 *    -sets up shared memory (SharedMemory) for inter-process communication
 *    -initializes an array of semaphores (semaphores) for parent-child synchronization
 *    -initializes a separate semaphore (parentNotificationSemaphore) for child-to-parent notifications
 *    -keeps track of active child processes, their activation and termination times, and their assigned semaphores
 *
 * 2. command execution:
 * -----------------------
 *    -processes commands from the configuration file at the appropriate timestamps
 *      -SPAWN: creates a new child process, assigns a semaphore, and records its details
 *      -TERMINATE: sends a termination message to the specified child process, waits for its exit, and logs its active time
 *      -EXIT: terminates all remaining active child processes and exits the simulation
 *    -for non-existent or inactive processes, logs a warning when a TERMINATE command is issued
 *
 * 3. message transmission:
 * ------------------------
 *    -sends random lines from the text file to randomly selected active child processes
 *    -waits for the child process to confirm message processing via the parentNotificationSemaphore
 *
 * 4. cleanup:
 * ------------
 *    -ensures proper cleanup of all resources, including shared memory and semaphores, to avoid resource leaks
 *    -logs simulation completion and exits gracefully
 *
 * parameters:
 * ---------------
 * -config_file:input configuration file containing SPAWN, TERMINATE, and EXIT commands
 * -text_file:input text file from which random lines are sent to child processes
 * -M:maximum number of child processes allowed (also defines the size of the semaphore array)
 */
void parentProcess(const char *config_file, const char *text_file, int M) {
    pid_t children[M]; 
    char childLabels[M][10]; 
    int childSemArr[M+1];
    int activationTime[M];
    int terminationTime[M]; 
    memset(children, 0, sizeof(children));
    memset(childLabels, 0, sizeof(childLabels));
    memset(childSemArr, -1, sizeof(childSemArr));
    memset(activationTime, -1, sizeof(activationTime));
    memset(terminationTime, -1, sizeof(terminationTime));

    int numOfConfigLines = 0; 
    ConfigEntry *commands = configInfo(config_file, &numOfConfigLines);
    int quitTimestamp = -1;

    for (int i = 0; i < numOfConfigLines; ++i) {
        if (commands[i].command == 'E') {
            quitTimestamp = commands[i].timestamp;
            break;
        }
    }

    if (quitTimestamp == -1) {
        fprintf(stderr, "Error: EXIT command not found in configuration file.\n");
        free(commands);
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(text_file, "r");
    if (!fp) {
        perror("Failed to open text file");
        free(commands);
        exit(EXIT_FAILURE);
    }

    int numOfLinesInText = lineCount(fp); 
    SharedMemory *sharedMem = sharedMemSetup();
    sem_t *semaphores[M]; 
    sem_t *parentNotificationSemaphore; 
    semInit(semaphores, M, &parentNotificationSemaphore);
    int activeChildren = 0; 
    for (int currTime = 0; currTime <= quitTimestamp; ++currTime) {
        for (int i = 0; i < numOfConfigLines; ++i) {
            if (commands[i].timestamp == currTime) { 
                char *label = commands[i].processLabel; 
                char command = commands[i].command;
				//spawn a new child process
                if (command == 'S' && activeChildren < M) {
                    int freeSlot = freeSlotFinder(children, M); 
                    if (freeSlot != -1) {
                        pid_t child_pid = fork(); 
                        if (child_pid == 0) {
                            child(semaphores, freeSlot, sharedMem, currTime, parentNotificationSemaphore); 
                            exit(EXIT_SUCCESS);
                        } else if (child_pid > 0) { 
                            children[freeSlot] = child_pid;
                            strncpy(childLabels[freeSlot], label, 10);
                            childSemArr[freeSlot] = freeSlot;
                            activationTime[freeSlot] = currTime;
                            activeChildren++;
                            printf("\n[t = %d] Spawned process %s (PID: %d)\n", currTime, label, child_pid);
                        }
                    }
                } else if (command == 'T') { 
                    int childIndx = childLabelFinder(childLabels, label, M); //first of we find the child index
                    if (childIndx != -1) {
                        terminationTime[childIndx] = currTime; //then update the shared memory
                        sharedMem->endsInTimestamp = currTime;
                        strcpy(sharedMem->sharedSpace, "TERMINATE");//set a terminate message, as asked
                        printf("\n[t = %d] Parent sent TERMINATE message to child[%d]\n", currTime, childIndx);
                        sem_post(semaphores[childIndx]); //signaling semaphore to unblock the child
                        sem_wait(parentNotificationSemaphore); // waiting for child to notify the parent
                        waitpid(children[childIndx], NULL, 0); //and the child process to exit
                        printf("[t = %d] Child[%d] has terminated.\n", currTime, childIndx);
                        children[childIndx] = 0;  //marking the slot free for reuse
                        activeChildren--; 
                    }else {
                        //covers the case of command issued for a non-existent or inactive process
                        printf("[t = %d] Warning: Terminate command issued for non-existent or inactive process: %s\n",
                               currTime, label);
                    }
                }
            }
        }

        if (activeChildren > 0) {
            int randChildIndx = randomActiveChildrenSelection(children, M);
            if (randChildIndx != -1 && numOfLinesInText > 0) {
                char line[MAX_LINE_SIZE];
                randomLineSelection(fp, line, numOfLinesInText);
                strcpy(sharedMem->sharedSpace, line); //we store the line in shared memory
                sharedMem->endsInTimestamp = currTime;
                sharedMem->activeChildIndx = randChildIndx;
                printf("[t = %d] Parent sent message to child[%d]: %s", currTime, randChildIndx, line);
                sem_post(semaphores[randChildIndx]); 
                sem_wait(parentNotificationSemaphore); 
            }
        }
    }
	//here we terminate all remaining active child processes that have not terminated with exit
    for (int i = 0; i < M; ++i) {
        if (children[i] > 0) {
            terminationTime[i] = quitTimestamp; 
            sharedMem->endsInTimestamp = quitTimestamp; 
            strcpy(sharedMem->sharedSpace, "TERMINATE"); 
            printf("\n[t = %d] Parent sent TERMINATE message to child[%d]\n", quitTimestamp, i);
            sem_post(semaphores[i]); 
            sem_wait(parentNotificationSemaphore); 
            waitpid(children[i], NULL, 0); 
            printf("[t = %d] Child[%d] has terminated.\n", quitTimestamp, i);
        }
    }

    rescourceCleanup(sharedMem, semaphores, M, parentNotificationSemaphore); 
    fclose(fp);
    free(commands);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <config_file> <text_file> <M>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *config_file = argv[1];
    const char *text_file = argv[2];
    int M = atoi(argv[3]);

    if (M <= 0) {
        fprintf(stderr, "Error: M must be a positive integer.\n");
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));

    parentProcess(config_file, text_file, M);

    return 0;
}

