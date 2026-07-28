#include <stdio.h>

#include "adcs_target.h"

static AdcsTarget current_target;

void adcs_target_set(
    AdcsTarget target
)
{
    current_target = target;

    printf(
        "[ADCS TARGET] Target changed to %d.\n"
    );
}

AdcsTarget adcs_target_get(void)
{
    return current_target;
}