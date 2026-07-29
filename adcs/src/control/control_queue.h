#ifndef CONTROL_QUEUE_H
#define CONTROL_QUEUE_H

#include "control_request.h"

void control_queue_initialize(void);

int control_queue_push(
    ControlRequest request
);

int control_queue_pop(

    ControlRequest *request
);

#endif