################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Platform/Src/adcs_application.c \
../Platform/Src/adcs_board.c \
../Platform/Src/adcs_csp_stm32.c \
../Platform/Src/adcs_environment_stm32.c \
../Platform/Src/adcs_newlib_locks.c \
../Platform/Src/adcs_sensors_unavailable.c 

OBJS += \
./Platform/Src/adcs_application.o \
./Platform/Src/adcs_board.o \
./Platform/Src/adcs_csp_stm32.o \
./Platform/Src/adcs_environment_stm32.o \
./Platform/Src/adcs_newlib_locks.o \
./Platform/Src/adcs_sensors_unavailable.o 

C_DEPS += \
./Platform/Src/adcs_application.d \
./Platform/Src/adcs_board.d \
./Platform/Src/adcs_csp_stm32.d \
./Platform/Src/adcs_environment_stm32.d \
./Platform/Src/adcs_newlib_locks.d \
./Platform/Src/adcs_sensors_unavailable.d 


# Each subdirectory must supply rules for building sources it contributes
Platform/Src/%.o Platform/Src/%.su Platform/Src/%.cyclo: ../Platform/Src/%.c Platform/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Platform-2f-Src

clean-Platform-2f-Src:
	-$(RM) ./Platform/Src/adcs_application.cyclo ./Platform/Src/adcs_application.d ./Platform/Src/adcs_application.o ./Platform/Src/adcs_application.su ./Platform/Src/adcs_board.cyclo ./Platform/Src/adcs_board.d ./Platform/Src/adcs_board.o ./Platform/Src/adcs_board.su ./Platform/Src/adcs_csp_stm32.cyclo ./Platform/Src/adcs_csp_stm32.d ./Platform/Src/adcs_csp_stm32.o ./Platform/Src/adcs_csp_stm32.su ./Platform/Src/adcs_environment_stm32.cyclo ./Platform/Src/adcs_environment_stm32.d ./Platform/Src/adcs_environment_stm32.o ./Platform/Src/adcs_environment_stm32.su ./Platform/Src/adcs_newlib_locks.cyclo ./Platform/Src/adcs_newlib_locks.d ./Platform/Src/adcs_newlib_locks.o ./Platform/Src/adcs_newlib_locks.su ./Platform/Src/adcs_sensors_unavailable.cyclo ./Platform/Src/adcs_sensors_unavailable.d ./Platform/Src/adcs_sensors_unavailable.o ./Platform/Src/adcs_sensors_unavailable.su

.PHONY: clean-Platform-2f-Src

