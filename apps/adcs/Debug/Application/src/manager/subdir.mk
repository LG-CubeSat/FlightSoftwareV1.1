################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Application/src/manager/adcs_manager.c \
../Application/src/manager/fault_manager.c 

OBJS += \
./Application/src/manager/adcs_manager.o \
./Application/src/manager/fault_manager.o 

C_DEPS += \
./Application/src/manager/adcs_manager.d \
./Application/src/manager/fault_manager.d 


# Each subdirectory must supply rules for building sources it contributes
Application/src/manager/%.o Application/src/manager/%.su Application/src/manager/%.cyclo: ../Application/src/manager/%.c Application/src/manager/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Application-2f-src-2f-manager

clean-Application-2f-src-2f-manager:
	-$(RM) ./Application/src/manager/adcs_manager.cyclo ./Application/src/manager/adcs_manager.d ./Application/src/manager/adcs_manager.o ./Application/src/manager/adcs_manager.su ./Application/src/manager/fault_manager.cyclo ./Application/src/manager/fault_manager.d ./Application/src/manager/fault_manager.o ./Application/src/manager/fault_manager.su

.PHONY: clean-Application-2f-src-2f-manager

