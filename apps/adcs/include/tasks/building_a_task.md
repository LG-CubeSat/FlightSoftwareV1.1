Every task should have a pretty similar design:

1. Read or receive some sort of info (i.e. Sensor data, Recieve Packet). DATA

2. Do something to/with that data. (i.e. update state, math). FUNCTION

3. Publish or Send the result of your FUNCTION (i.e. send packet, send results, publish attitude) SEND

4. Delay some amount of time (i.e. 10 milliseconds, 50 milliseconds) DELAY

This is the pattern:
DATA -> FUNCTION -> SEND -> DELAY ... *repeat*

To make a task use FreeRTOS xTaskCreate along with a TaskType and Buffer.
You will also want a queue.

Ensure you have both a:
__________________
void some_task_init(void);

void some_task(void *argument);
__________________

Init:
- Creates Task
- Obtains queue handles
- Other task specific inits

Task:
- No init at top of the function
- TickType_t lastWake = xTaskGetTickCount();
- While superloop contains the actual actions of the task
- DON'T Put any actual math, compute, algorithm, etc. Rather use functions that call other files.