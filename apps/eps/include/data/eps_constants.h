#ifndef EPS_CONSTANTS
#define EPS_CONSTANTS


/*

eps constants work with fault manager, for V1 & V2 there will be very simple
constants and as the code gets more complicated and there are more errors that could occur (hw and sw)
we will add more constants and more stuff to fault manager to prevent critical failure

*/

#include <math.h>


/*--------------------------------------------------------------------*/
//CONSTANTS (CAN BE TWEAKED, NOT HW RELATED)
/*--------------------------------------------------------------------*/

#define SENSOR_MAX_AGE_MS	200U	//Maximum age of a usable sensor snapshot
#define MAX_SAMPLE_INTERVAL_MS	300U	//Maximum interval accepted for accumulation

#define MIN_DISCHARGE_POWER_W	0.1f	//Minimum discharge power for time-to-empty
#define MIN_CHARGE_POWER_W	0.1f	//Minimum charging power for time-to-full

/*--------------------------------------------------------------------*/
//TO BE IMPLEMENTED W/ HW (IF THERE IS A VALUE RN, IT IS FOR TETSING)
/*--------------------------------------------------------------------*/

#define BATTERY_USABLE_CAPACITY_WH	20	//Usable battery capacity
#define BATTERY_INITIAL_ENERGY_WH	10	//Battery energy when tracking starts

#define WARNING_BATTERY_VOLTAGE_HIGH 25U //Send OBC & Fault Manager Warning for High Voltage
#define CRITICAL_BATTERY_VOLTAGE_HIGH 30U //Do Something About Battery Voltage (CRITICAL)
#define WARNING_BATTERY_CURRENT_HIGH 25U //Send OBC & Fault Manager Warning for High Current
#define CRITICAL_BATTERY_CURRENT_HIGH 30U //Do Something About Battery Current (CRITICAL)

#define WARNING_BATTERY_VOLTAGE_LOW 5U //Send OBC & Fault Manager Warning for High Voltage
#define CRITICAL_BATTERY_VOLTAGE_LOW 3U //Do Something About Battery Voltage (CRITICAL)
#define WARNING_BATTERY_CURRENT_LOW 0.25f //Send OBC & Fault Manager Warning for High Current
#define CRITICAL_BATTERY_CURRENT_LOW 0.1f //Do Something About Battery Current (CRITICAL)


#endif //EPS CONSTNATS