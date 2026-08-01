#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "control_queue.h"
#include "control_request.h"

#define CONTROL_QUEUE_SIZE 8 

static QueueHandle_t xControlQueue;
static StaticQueue_t xStaticQueue;
static uint8_t ucQueueStorageArea[CONTROL_QUEUE_SIZE * sizeof(ControlRequest)] __attribute__((aligned(8)));

void control_queue_initialize(void)
{
    xControlQueue = xQueueCreateStatic(CONTROL_QUEUE_SIZE, sizeof(ControlRequest), ucQueueStorageArea, &xStaticQueue);
    if (xControlQueue == NULL) {
        printf("[CONTROL QUEUE] ERROR: Failed to create control queue.\n");
    } else {
        printf("[CONTROL QUEUE] RTOS Control Queue Initialized.\n");
    }
}

int control_queue_push(
    ControlRequest request
)
{
    return (int)xQueueSend(xControlQueue, &request, 0);
}


int control_queue_pop(
    ControlRequest *request
)
{
    return (int)xQueueReceive(xControlQueue, request, 0);
}
