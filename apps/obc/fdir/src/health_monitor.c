#include "health_monitor.h"

/*
Watches the health of each subsystem:
Turns the flags given from watchdog and limit checker into real issue states (how severe the problem is)
*/