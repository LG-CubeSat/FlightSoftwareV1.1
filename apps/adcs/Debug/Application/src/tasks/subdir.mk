################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Application/src/tasks/command_task.c \
../Application/src/tasks/control_task.c \
../Application/src/tasks/estimation_task.c \
../Application/src/tasks/housekeeping_task.c \
../Application/src/tasks/sensor_task.c \
../Application/src/tasks/telemetry_task.c 

OBJS += \
./Application/src/tasks/command_task.o \
./Application/src/tasks/control_task.o \
./Application/src/tasks/estimation_task.o \
./Application/src/tasks/housekeeping_task.o \
./Application/src/tasks/sensor_task.o \
./Application/src/tasks/telemetry_task.o 

C_DEPS += \
./Application/src/tasks/command_task.d \
./Application/src/tasks/control_task.d \
./Application/src/tasks/estimation_task.d \
./Application/src/tasks/housekeeping_task.d \
./Application/src/tasks/sensor_task.d \
./Application/src/tasks/telemetry_task.d 


# Each subdirectory must supply rules for building sources it contributes
Application/src/tasks/%.o Application/src/tasks/%.su Application/src/tasks/%.cyclo: ../Application/src/tasks/%.c Application/src/tasks/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Application-2f-src-2f-tasks

clean-Application-2f-src-2f-tasks:
	-$(RM) ./Application/src/tasks/command_task.cyclo ./Application/src/tasks/command_task.d ./Application/src/tasks/command_task.o ./Application/src/tasks/command_task.su ./Application/src/tasks/control_task.cyclo ./Application/src/tasks/control_task.d ./Application/src/tasks/control_task.o ./Application/src/tasks/control_task.su ./Application/src/tasks/estimation_task.cyclo ./Application/src/tasks/estimation_task.d ./Application/src/tasks/estimation_task.o ./Application/src/tasks/estimation_task.su ./Application/src/tasks/housekeeping_task.cyclo ./Application/src/tasks/housekeeping_task.d ./Application/src/tasks/housekeeping_task.o ./Application/src/tasks/housekeeping_task.su ./Application/src/tasks/sensor_task.cyclo ./Application/src/tasks/sensor_task.d ./Application/src/tasks/sensor_task.o ./Application/src/tasks/sensor_task.su ./Application/src/tasks/telemetry_task.cyclo ./Application/src/tasks/telemetry_task.d ./Application/src/tasks/telemetry_task.o ./Application/src/tasks/telemetry_task.su

.PHONY: clean-Application-2f-src-2f-tasks

