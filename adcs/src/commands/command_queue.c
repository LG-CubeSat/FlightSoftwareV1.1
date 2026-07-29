#include <stdio.h>
#include "command_queue.h"

#define COMMAND_QUEUE_SIZE 8

/*
Notes:
- Receives a command from the parser
- Uses a circular queue to effeciently store commands without shifting the entire queue
- Uses FIFO (first in first out)
- Simply stores commands allowing them to be grabbed or added
*/

static CommandMessage queue[COMMAND_QUEUE_SIZE];

static int head;
static int tail;
static int count;

void command_queue_initialize(void)
{
    head = 0;
    tail = 0;
    count = 0;

    printf("[QUEUE] Initialized.\n");
}

// add command to queue
int command_queue_push(
    CommandMessage command
)
{
    // check if queue is full, if it is then don't add
    if (count >= COMMAND_QUEUE_SIZE) {
        printf("[QUEUE] ERROR: Queue full.\n");

        return 0;
    }
    // append the end value with the command
    queue[tail] = command;
    tail++; // shift the location of the end value

    // loop around if needed
    if (tail >= COMMAND_QUEUE_SIZE)
    {
        tail = 0;
    }
    count++; // change size

    return 1;
}

// grab the next command, and remove from queue
int command_queue_pop(
    CommandMessage *command
)
{
    // check if empty
    if (count == 0) {
        return 0;
    }

    // grab the command to use
    *command = queue[head];
    head++; // move where we grab commands

    // loop around
    if (head >= COMMAND_QUEUE_SIZE)
    {
        head = 0;
    }
    
    count--; // decrease size

    return 1;
}