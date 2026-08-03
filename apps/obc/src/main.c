#include <stdio.h>
#include "../../../platform/sim/include/v_bus.h"
#include <sys/wait.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 128

int main(void)
{
    printf("OBC On...");
    VBus_t v_bus;
    v_bus = create_v_bus();
    v_bus.initialize(1);

    uint8_t buffer[MAX_BUFFER_SIZE];

    while (1)
    {    
        int bytes_received = v_bus.receive(buffer, MAX_BUFFER_SIZE);
        if (bytes_received > 0)
        {
            // Use %.*s to print only the exact number of bytes received
            // This avoids buffer overruns if the data lacks a null terminator
            printf("[OBC MAIN] Received %d bytes: %.*s\n", bytes_received, bytes_received, buffer);
        }
    }
    return 0;
}