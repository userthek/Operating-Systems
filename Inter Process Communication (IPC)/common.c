#include "common.h"
#include <fcntl.h>

/**
 * freeSlotFinder
 * #################################################################################################
 * identifies the first available (free) slot in the children array
 *  A free slot is represented by a value of 0 in the array, which implies no active child process is occupying that slot.
 *
 * ->Parameters
 * ------------
 * -children: array containing the PIDs of child processes
 * -M: maximum size of the array
 *
 * ->Returns
 * ---------
 * -the index of the first free slot
 * -or -1 if no slot is available
 */
int freeSlotFinder(pid_t *children, int M) {
    for (int i = 0; i < M; i++) {
        if (children[i] == 0) { 
            return i;
        }
    }
    return -1; 
}

/**
 * childLabelFinder
 * #################################################################################################
 * searches for a specific child process based on its label in the childLabels array
 *  labels are unique identifiers assigned to each child (e.g., "C1", "C2").
 *
 * ->Parameters
 * ------------
 * -childLabels: array containing labels of child processes
 * -label: the label to search for
 * -M: maximum size of the array
 *
 * ->Returns
 * ---------
 * -the index of the child process matching the label
 * -or -1 if the label is not found
 */
int childLabelFinder(char childLabels[][10], const char *label, int M) {
    for (int i = 0; i < M; i++) {
        if (strcmp(childLabels[i], label) == 0) { //Match found
            return i;
        }
    }
    return -1; 
}

/**
 * lineCount
 * #################################################################################################
 * counts the total number of lines in the given file. This is useful for selecting random lines later
 * or for determining file statistics.
 *
 * ->Parameters
 * ------------
 * -fp: file pointer to the open file
 *
 * ->Returns
 * ---------
 * -total number of lines in the file
 */
int lineCount(FILE *fp) {
    int totalLines = 0;
    char buffer[MAX_LINE_SIZE];
    rewind(fp); //here we reset the file pointer to the beginning of the file
    while (fgets(buffer, sizeof(buffer), fp)) { //reading line by line
        totalLines++;
    }
    rewind(fp); //we most likely would want to reuse it so we reset the file ptr
    return totalLines;
}

/**
 * randomActiveChildrenSelection
 * #################################################################################################
 * selects a random active child process from the array of children. Active children are represented
 * by non-zero PID values in the array.
 *
 * ->Parameters
 * ------------
 * -children:array containing the PIDs of child processes
 * -M:maximum size of the array
 *
 * ->Returns
 * ---------
 * -index of a randomly selected active child
 * -or -1 if no active child exists
 */
int randomActiveChildrenSelection(pid_t *children, int M) {
    int activeIndexes[M];
    int countOfActive = 0;
    for (int i = 0; i < M; i++) {
        if (children[i] > 0) { 
            activeIndexes[countOfActive++] = i;
        }
    }
    if (countOfActive == 0) {
        return -1;
    }
    return activeIndexes[rand() % countOfActive];
}

/**
 * randomLineSelection
 * #################################################################################################
 * retrieves a random line from the file whose selection is selected based on a random index, ensuring
 * uniform distribution across all lines.
 *
 * ->Parameters
 * ------------
 * -fp:file pointer to the open file
 * -buffer:buffer to store the randomly selected line
 * -totalLines: total number of lines in the file
 */
void randomLineSelection(FILE *fp, char *buffer, int totalLines) {
    int randLine = rand() % totalLines; 
    rewind(fp); 
    for (int i = 0; i <= randLine; i++) { 
        fgets(buffer, MAX_LINE_SIZE, fp);
    }
}

/**
 * sharedMemSetup
 * #################################################################################################
 * initializes and sets up the shared memory segment for inter-process communication (IPC)
 *
 * ->Returns
 * ---------
 * -pointer to the initialized shared memory structure
 * -if an error occurs, the program terminates with an appropriate message
 */
SharedMemory* sharedMemSetup() {
    key_t key = ftok("shmfile", 65); //generation of unique Key for SharedMem
    int shmid = shmget(key, sizeof(SharedMemory), 0666 | IPC_CREAT); //creation of sharedMem
    if (shmid == -1) {
        perror("Failed to create shared memory");
        exit(EXIT_FAILURE);
    }

    SharedMemory *sharedMem = (SharedMemory *)shmat(shmid, NULL, 0); 
    if (sharedMem == (void *)-1) {
        perror("Failed to attach shared memory");
        exit(EXIT_FAILURE);
    }

    //we iniria;ize the fields of the sharedMem structure
    sharedMem->activeChildIndx = -1; //at start no child exists
    sharedMem->endsInTimestamp = -1;   //no termination timestamp by default
    memset(sharedMem->sharedSpace, 0, sizeof(sharedMem->sharedSpace)); //cleanup
    return sharedMem;
}

/**
 * rescourceCleanup
 * #################################################################################################
 * cleans up resources such as shared memory and semaphores after their usage
 * ensures proper release of system resources to prevent resource leaks
 *
 * parameters:
 * -sharedMem:pointer to the shared memory structure used for inter-process communication
 * -semaphores:array of semaphore pointers used for synchronization between parent and child processes
 * -M:number of semaphores in the array (typically corresponds to the number of child processes)
 * -parentNotificationSemaphore:pointer to the semaphore used for parent-to-child notifications
 */

void rescourceCleanup(SharedMemory *sharedMem, sem_t *semaphores[], int M, sem_t *parentNotificationSemaphore) {
    shmdt(sharedMem);
    shmctl(shmget(ftok("shmfile", 65), sizeof(SharedMemory), 0666 | IPC_CREAT), IPC_RMID, NULL);
    for (int i = 0; i < M; i++) {
        if (semaphores[i]) {
            sem_close(semaphores[i]);
            char semName[32];
            snprintf(semName, sizeof(semName), "/semaphore_%d", i);
            sem_unlink(semName);
        }
    }
    if (parentNotificationSemaphore) {
        sem_close(parentNotificationSemaphore);
        sem_unlink("/parent_notification");
    }
}


/**
 * semInit
 * #################################################################################################
 * initializes an array of semaphores for synchronization between the parent and child processes
 * the function also initializes a separate semaphore for parent-to-child notifications
 * 
 * each semaphore is uniquely named for consistent identification and initialized to 0
 * 
 * parameters:
 * -semaphores:array to store initialized semaphore pointers for child processes
 * -M:number of semaphores to initialize for child processes
 * -parentNotificationSemaphore:pointer to store the initialized parent notification semaphore
 */

void semInit(sem_t *semaphores[], int M, sem_t **parentNotificationSemaphore) {
    for (int i = 0; i < M; i++) {
        char semName[32];
        snprintf(semName, sizeof(semName), "/semaphore_%d", i);
        semaphores[i] = sem_open(semName, O_CREAT, 0644, 0);
        if (semaphores[i] == SEM_FAILED) {
            perror("Failed to create semaphore");
            exit(EXIT_FAILURE);
        }
    }
    
    *parentNotificationSemaphore = sem_open("/parent_notification", O_CREAT, 0644, 0);
    if (*parentNotificationSemaphore == SEM_FAILED) {
        perror("Failed to create parent notification semaphore");
        exit(EXIT_FAILURE);
    }
}
