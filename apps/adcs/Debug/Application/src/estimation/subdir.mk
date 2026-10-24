################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Application/src/estimation/attitude_estimator.c \
../Application/src/estimation/kalman_filter.c \
../Application/src/estimation/reference_vectors.c 

OBJS += \
./Application/src/estimation/attitude_estimator.o \
./Application/src/estimation/kalman_filter.o \
./Application/src/estimation/reference_vectors.o 

C_DEPS += \
./Application/src/estimation/attitude_estimator.d \
./Application/src/estimation/kalman_filter.d \
./Application/src/estimation/reference_vectors.d 


# Each subdirectory must supply rules for building sources it contributes
Application/src/estimation/%.o Application/src/estimation/%.su Application/src/estimation/%.cyclo: ../Application/src/estimation/%.c Application/src/estimation/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Application-2f-src-2f-estimation

clean-Application-2f-src-2f-estimation:
	-$(RM) ./Application/src/estimation/attitude_estimator.cyclo ./Application/src/estimation/attitude_estimator.d ./Application/src/estimation/attitude_estimator.o ./Application/src/estimation/attitude_estimator.su ./Application/src/estimation/kalman_filter.cyclo ./Application/src/estimation/kalman_filter.d ./Application/src/estimation/kalman_filter.o ./Application/src/estimation/kalman_filter.su ./Application/src/estimation/reference_vectors.cyclo ./Application/src/estimation/reference_vectors.d ./Application/src/estimation/reference_vectors.o ./Application/src/estimation/reference_vectors.su

.PHONY: clean-Application-2f-src-2f-estimation

