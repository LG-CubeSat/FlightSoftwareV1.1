#include <stdint.h>

#ifndef thermal_functions_h
#define thermal_functions_h


float farenheitToCelsius(float farenheit) {

    return (farenheit - 32) * 5/9;

}
float celsiusToFarenheit(float celsius) {

    return (celsius * 9/5) +32;

}




#endif // thermal_functions_h