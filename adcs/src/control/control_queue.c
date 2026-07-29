#include <stdio.h>

#include "control_queue.h"

#define CONTROL_QUEUE_SIZE 8 

static ControlRequest queue[CONTROL_QUEUE_SIZE];

static int head;
static int tail;
static int count;

void control_queue_initialize(void)
{
    head = 0;

    tail = 0;

    count = 0;

    printf(
        "[CONTROL QUEUE] Initialized.\n"
    );
}

int control_queue_push(
    ControlRequest request
)
{
    if (count >= CONTROL_QUEUE_SIZE) {
        printf("[CONTROL QUEUE] ERROR: Queue full.\n");
    }

    return 0;

    queue[tail] = request;

    tail++;

    if (tail >= CONTROL_QUEUE_SIZE)
    {
        tail = 0;
    }

    count++;

    return 1;
}


int control_queue_pop(
    ControlRequest *request
)
{
    if (count == 0)
    {
        return 0;
    }

    *request = queue[head];

    head++;

    if (head >= CONTROL_QUEUE_SIZE)
    {
        head = 0;
    }

    count--;

    return 1;
}
