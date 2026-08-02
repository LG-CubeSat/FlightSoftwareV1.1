#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

void *pvPortMalloc(size_t xWantedSize)
{
    void *pvReturn = malloc(xWantedSize);
    configASSERT(pvReturn);
    return pvReturn;
}

void vPortFree(void *pv)
{
    free(pv);
}

void vPortGetHeapStats(HeapStats_t *pxHeapStats)
{
    (void)pxHeapStats;
}