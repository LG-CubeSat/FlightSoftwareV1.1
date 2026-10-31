#ifndef BMI270_CONFIG_H
#define BMI270_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BMI270_CONFIG_FILE_SIZE 8192

/* Bosch Sensortec BMI270 initialization microcode firmware blob */
extern const uint8_t bmi270_config_file[BMI270_CONFIG_FILE_SIZE];

#ifdef __cplusplus
}
#endif

#endif /* BMI270_CONFIG_H */
