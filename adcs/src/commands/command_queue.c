#include <stdio.h>
#include "command_queue.h"

#define COMMAND_QUEUE_SIZE 8

#include "FreeRTOS.h"
#include "queue.h"
/*
Notes:
- Receives a command from the parser
- Uses a circular queue to effeciently store commands without shifting the entire queue
- Uses FIFO (first in first out)
- Simply stores commands allowing them to be grabbed or added
*/

static StaticQueue_t xStaticQueue;

static QueueHandle_t xCommandQueue;
static uint8_t ucQueueStorageArea[COMMAND_QUEUE_SIZE * sizeof(CommandMessage)];

void command_queue_initialize(void) {
    xCommandQueue = xQueueCreateStatic(COMMAND_QUEUE_SIZE, sizeof(CommandMessage), ucQueueStorageArea, &xStaticQueue);
    printf("[QUEUE] RTOS Queue Initialized.\n");
}

// add command to queue
int command_queue_push(
    CommandMessage command
)
{
    xQueueSend()
}

// grab the next command, and remove from queue
int command_queue_pop(
    CommandMessage *command
)
{
    
}