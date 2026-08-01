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

static QueueHandle_t xCommandQueue;

static StaticQueue_t xStaticQueue;
static uint8_t ucQueueStorageArea[COMMAND_QUEUE_SIZE * sizeof(CommandMessage)] __attribute__((aligned(8)));

void command_queue_initialize(void) {
    xCommandQueue = xQueueCreateStatic(COMMAND_QUEUE_SIZE, sizeof(CommandMessage), ucQueueStorageArea, &xStaticQueue);
    if (xCommandQueue == NULL) {
        printf("[COMMAND QUEUE] ERROR: Failed to created command queue!\n");
    } else {
        printf("[COMMAND QUEUE] SUCCESS: RTOS Queue Initialized\n");
    }
}

// add command to queue
int command_queue_push(
    CommandMessage command
)
{
    return (int)xQueueSend(xCommandQueue, &command, 0);
}

// grab the next command, and remove from queue
int command_queue_pop(
    CommandMessage *command
)
{
    return (int)xQueueReceive(xCommandQueue, command, 0);
}