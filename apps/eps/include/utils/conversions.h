#ifndef CONVERSION_H
#define CONVERSION_H



float watts_to_milliwatts(float watts) {
    return watts * 1000;
}
float milliwatts_to_watts(float milliwatts) {
    return milliwatts / 1000;
}
float joules_to_watt_hours(float joules) {
    return joules / 3600;
}
float watt_hours_to_joules(float watt_hours) {
    return watt_hours * 3600;
}
float volts_to_millivolts(float volts) {
    return volts * 1000;
}
float millivolts_to_volts(float millivolts) {
    return millivolts / 1000;
}
float amps_to_milliamps(float amps) {
    return amps * 1000;
}
float milliamps_to_amps(float milliamps) {
    return milliamps / 1000;
}
float ohms_to_milliohms(float ohms) {
    return ohms * 1000;
}
float milliohms_to_ohms(float milliohms) {
    return milliohms / 1000;
}
float kelvin_to_celsius(float kelvin) {
    return kelvin - 273.15;
}
float celsius_to_kelvin(float celsius) {
    return celsius + 273.15;
}
float coulombs_to_ampere_seconds(float coulombs) {
    return coulombs;
}
float ampere_seconds_to_coulombs(float ampere_seconds) {
    return ampere_seconds;
}
float farads_to_microfarads(float farads) {
    return farads * 1000000;
}
float microfarads_to_farads(float microfarads) {
    return microfarads / 1000000;
}
float volts_to_abvolts(float volts) {
    return volts / 1.0f;
}
float abvolts_to_volts(float abvolts) {
    return abvolts * 1.0f;
}
float volts_to_statvolts(float volts) {
    return volts * 299.792458f;
}
float statvolts_to_volts(float statvolts) {
    return statvolts / 299.792458f;
}
float amps_to_abamps(float amps) {
    return amps / 1.0f;
}
float abamps_to_amps(float abamps) {
    return abamps * 1.0f;
}
float amps_to_statamps(float amps) {
    return amps * 0.1f;
}
float statamps_to_amps(float statamps) {
    return statamps * 10.0f;
}
float watts_to_megawatts(float watts) {
    return watts / 1000000.0f;
}
float megawatts_to_watts(float megawatts) {
    return megawatts * 1000000.0f;
}
float watts_to_gigawatts(float watts) {
    return watts / 1000000000.0f;
}
float gigawatts_to_watts(float gigawatts) {
    return gigawatts * 1000000000.0f;
}
float joules_to_megajoules(float joules) {
    return joules / 1000000.0f;
}
float megajoules_to_joules(float megajoules) {
    return megajoules * 1000000.0f;
}
float joules_to_gigajoules(float joules) {
    return joules / 1000000000.0f;
}
float gigajoules_to_joules(float gigajoules) {
    return gigajoules * 1000000000.0f;
}
float watt_hours_to_kilojoules(float watt_hours) {
    return watt_hours * 3.6f;
}
float kilojoules_to_watt_hours(float kilojoules) {
    return kilojoules / 3.6f;
}
float watt_hours_to_megajoules(float watt_hours) {
    return watt_hours * 0.0036f;
}
float megajoules_to_watt_hours(float megajoules) {
    return megajoules / 0.0036f;
}
float ampere_hours_to_kilocoulombs(float ampere_hours) {
    return ampere_hours * 3.6f;
}
float kilocoulombs_to_ampere_hours(float kilocoulombs) {
    return kilocoulombs / 3.6f;
}
float ampere_hours_to_millicoulombs(float ampere_hours) {
    return ampere_hours * 3600000.0f;
}
float millicoulombs_to_ampere_hours(float millicoulombs) {
    return millicoulombs / 3600000.0f;
}
float farads_to_abfarads(float farads) {
    return farads / 89875517923500.0f;
}
float abfarads_to_farads(float abfarads) {
    return abfarads * 89875517923500.0f;
}
float teslas_to_milliteslas(float teslas) {
    return teslas * 1000.0f;
}
float milliteslas_to_teslas(float milliteslas) {
    return milliteslas / 1000.0f;
}
float teslas_to_microteslas(float teslas) {
    return teslas * 1000000.0f;
}
float microteslas_to_teslas(float microteslas) {
    return microteslas / 1000000.0f;
}
float teslas_to_nanoteslas(float teslas) {
    return teslas * 1000000000.0f;
}
float nanoteslas_to_teslas(float nanoteslas) {
    return nanoteslas / 1000000000.0f;
}
float webers_to_milliwebers(float webers) {
    return webers * 1000.0f;
}
float milliwebers_to_webers(float milliwebers) {
    return milliwebers / 1000.0f;
}
float webers_to_microwebers(float webers) {
    return webers * 1000000.0f;
}
float microwebers_to_webers(float microwebers) {
    return microwebers / 1000000.0f;
}
float lumens_to_millilumens(float lumens) {
    return lumens * 1000.0f;
}
float millilumens_to_lumens(float millilumens) {
    return millilumens / 1000.0f;
}
float lux_to_millilux(float lux) {
    return lux * 1000.0f;
}
float millilux_to_lux(float millilux) {
    return millilux / 1000.0f;
}
float watts_per_meter_squared_to_milliwatts_per_meter_squared(float watts_per_meter_squared) {
    return watts_per_meter_squared * 1000.0f;
}
float milliwatts_per_meter_squared_to_watts_per_meter_squared(float milliwatts_per_meter_squared) {
    return milliwatts_per_meter_squared / 1000.0f;
}
float watts_per_meter_squared_to_microwatts_per_meter_squared(float watts_per_meter_squared) {
    return watts_per_meter_squared * 1000000.0f;
}
float microwatts_per_meter_squared_to_watts_per_meter_squared(float microwatts_per_meter_squared) {
    return microwatts_per_meter_squared / 1000000.0f;
}
float watts_per_meter_squared_to_milliwatts_per_centimeter_squared(float watts_per_meter_squared) {
    return watts_per_meter_squared / 10.0f;
}
float milliwatts_per_centimeter_squared_to_watts_per_meter_squared(float milliwatts_per_centimeter_squared) {
    return milliwatts_per_centimeter_squared * 10.0f;
}
float radians_per_second_to_degrees_per_second(float radians_per_second) {
    return radians_per_second * 57.2957795f;
}
float degrees_per_second_to_radians_per_second(float degrees_per_second) {
    return degrees_per_second * 0.0174532925f;
}
float revolutions_per_minute_to_hertz(float revolutions_per_minute) {
    return revolutions_per_minute / 60.0f;
}
float hertz_to_revolutions_per_minute(float hertz) {
    return hertz * 60.0f;
}
float kilohertz_to_millihertz(float kilohertz) {
    return kilohertz * 1000000.0f;
}
float millihertz_to_kilohertz(float millihertz) {
    return millihertz / 1000000.0f;
}
float gigahertz_to_megahertz(float gigahertz) {
    return gigahertz * 1000.0f;
}
float megahertz_to_gigahertz(float megahertz) {
    return megahertz / 1000.0f;
}
float nanoseconds_to_picoseconds(float nanoseconds) {
    return nanoseconds * 1000.0f;
}
float picoseconds_to_nanoseconds(float picoseconds) {
    return picoseconds / 1000.0f;
}
float milliseconds_to_minutes(float milliseconds) {
    return milliseconds / 60000.0f;
}
float minutes_to_milliseconds(float minutes) {
    return minutes * 60000.0f;
}
float hours_to_milliseconds(float hours) {
    return hours * 3600000.0f;
}
float milliseconds_to_hours(float milliseconds) {
    return milliseconds / 3600000.0f;
}
float days_to_minutes(float days) {
    return days * 1440.0f;
}
float minutes_to_days(float minutes) {
    return minutes / 1440.0f;
}
float microhenries_to_nanohenries(float microhenries) {
    return microhenries * 1000.0f;
}
float nanohenries_to_microhenries(float nanohenries) {
    return nanohenries / 1000.0f;
}
float henries_to_millihenries(float henries) {
    return henries * 1000.0f;
}
float millihenries_to_henries(float millihenries) {
    return millihenries / 1000.0f;
}
float microfarads_to_nanofarads(float microfarads) {
    return microfarads * 1000.0f;
}
float nanofarads_to_microfarads(float nanofarads) {
    return nanofarads / 1000.0f;
}
float picofarads_to_femtofarads(float picofarads) {
    return picofarads * 1000.0f;
}
float femtofarads_to_picofarads(float femtofarads) {
    return femtofarads / 1000.0f;
}
float megaohms_to_gigaohms(float megaohms) {
    return megaohms / 1000.0f;
}
float gigaohms_to_megaohms(float gigaohms) {
    return gigaohms * 1000.0f;
}
float kilohms_to_milliohms(float kilohms) {
    return kilohms * 1000000.0f;
}
float milliohms_to_kilohms(float milliohms) {
    return milliohms / 1000000.0f;
}
float siemens_to_microsiemens(float siemens) {
    return siemens * 1000000.0f;
}
float microsiemens_to_siemens(float microsiemens) {
    return microsiemens / 1000000.0f;
}
float siemens_to_nanosiemens(float siemens) {
    return siemens * 1000000000.0f;
}
float nanosiemens_to_siemens(float nanosiemens) {
    return nanosiemens / 1000000000.0f;
}
float coulombs_to_nanocoulombs(float coulombs) {
    return coulombs * 1000000000.0f;
}
float nanocoulombs_to_coulombs(float nanocoulombs) {
    return nanocoulombs / 1000000000.0f;
}
float coulombs_to_picocoulombs(float coulombs) {
    return coulombs * 1000000000000.0f;
}
float picocoulombs_to_coulombs(float picocoulombs) {
    return picocoulombs / 1000000000000.0f;
}
float milliamps_to_nanoamps(float milliamps) {
    return milliamps * 1000000.0f;
}
float nanoamps_to_milliamps(float nanoamps) {
    return nanoamps / 1000000.0f;
}
float volts_to_megavolts(float volts) {
    return volts / 1000000.0f;
}
float megavolts_to_volts(float megavolts) {
    return megavolts * 1000000.0f;
}
float milliwatts_to_nanowatts(float milliwatts) {
    return milliwatts * 1000000.0f;
}
float nanowatts_to_milliwatts(float nanowatts) {
    return nanowatts / 1000000.0f;
}
float kilowatts_to_microwatts(float kilowatts) {
    return kilowatts * 1000000000.0f;
}
float microwatts_to_kilowatts(float microwatts) {
    return microwatts / 1000000000.0f;
}
float joules_to_nanojoules(float joules) {
    return joules * 1000000000.0f;
}
float nanojoules_to_joules(float nanojoules) {
    return nanojoules / 1000000000.0f;
}
float watt_hours_to_milliwatt_hours(float watt_hours) {
    return watt_hours * 1000.0f;
}
float milliwatt_hours_to_watt_hours(float milliwatt_hours) {
    return milliwatt_hours / 1000.0f;
}
float watt_hours_to_micro_watt_hours(float watt_hours) {
    return watt_hours * 1000000.0f;
}
float micro_watt_hours_to_watt_hours(float micro_watt_hours) {
    return micro_watt_hours / 1000000.0f;
}


#endif //CONVERSION_H

/*
 * Include conversions for alot of things including:
 *watts and milliwatts,
joules and watt-hours,
coulombs and ampere-seconds,
volts and millivolts,
amperes and milliamperes,
Kelvin and Celsius.

*/
