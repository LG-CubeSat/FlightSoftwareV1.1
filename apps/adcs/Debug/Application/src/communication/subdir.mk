################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Application/src/communication/command_handler.c \
../Application/src/communication/telemetry.c 

OBJS += \
./Application/src/communication/command_handler.o \
./Application/src/communication/telemetry.o 

C_DEPS += \
./Application/src/communication/command_handler.d \
./Application/src/communication/telemetry.d 


# Each subdirectory must supply rules for building sources it contributes
Application/src/communication/%.o Application/src/communication/%.su Application/src/communication/%.cyclo: ../Application/src/communication/%.c Application/src/communication/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Application-2f-src-2f-communication

clean-Application-2f-src-2f-communication:
	-$(RM) ./Application/src/communication/command_handler.cyclo ./Application/src/communication/command_handler.d ./Application/src/communication/command_handler.o ./Application/src/communication/command_handler.su ./Application/src/communication/telemetry.cyclo ./Application/src/communication/telemetry.d ./Application/src/communication/telemetry.o ./Application/src/communication/telemetry.su

.PHONY: clean-Application-2f-src-2f-communication

