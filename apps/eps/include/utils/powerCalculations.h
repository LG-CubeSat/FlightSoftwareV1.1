#ifndef POWERCALCULATIONS_H
#define POWERCALCULATIONS_H


float calculateWatts(float voltage, float amps) {
    return voltage * amps;
}
float calculateVoltage(float amps, float watts) {
    return watts / amps;
}
float calculateAmps(float watts, float voltage) {
    return watts / voltage;
}
float calculateResistance(float voltage, float current) {
    return voltage / current;
}
float calculateResistanceDrop(float current, float resistance) {
    return current * resistance;
}
float calculatePowerLoss(float current, float resistance) {
    return current * current * resistance;
}
float calculateEnergy(float power, float time) {
    return power * time;
}
float calculateAveragePower(float energy, float time) {
    return energy / time;
}


#endif //POWERCALCULATIONS_H
/*
 * Instant Power calculation
 * Energy is the integral of power over time
 * Average power over a time interval
 * Peak power calculation (max value in power profile)
 * Loss calculation
 * State of charge calculation
 * Battery efficiency calculation
 * etc
 *
 * This is just a few options for functions that are usable. There are alot more.
 * I don't know if battery is gonna provide needed info for this, but if it doesn't we can probably implement all of these.
 * They seem to be general math utils so we can probably just use a library or ai generate one (ofc if so check thoroughly)
*/
