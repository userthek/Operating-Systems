#pragma once

#include "common.h"

/* Child process function that generates random requests for lines, waits for segment
availability, records timing information, and writes the requested line to an output file.*/
void child(sem_t* seg_semaphores[], int M_requests, SharedMemory* sharedMem, int activation_time, sem_t *parentNotificationSemaphore);
