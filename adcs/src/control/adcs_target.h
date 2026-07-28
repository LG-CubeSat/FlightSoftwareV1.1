#ifndef ADCS_TARGET
#define ADCS_TARGET

typedef enum
{
    ADCS_TARGET_NONE = 0,
    ADCS_TARGET_SUN,
    ADCS_TARGET_EARTH,
    ADCS_TARGET_NADIR
} AdcsTarget;

void adcs_target_set(
    AdcsTarget target
);

AdcsTarget adcs_target_get(void);

#endif