################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../RTOS/ARM_CM4F/port.c 

OBJS += \
./RTOS/ARM_CM4F/port.o 

C_DEPS += \
./RTOS/ARM_CM4F/port.d 


# Each subdirectory must supply rules for building sources it contributes
RTOS/ARM_CM4F/%.o RTOS/ARM_CM4F/%.su RTOS/ARM_CM4F/%.cyclo: ../RTOS/ARM_CM4F/%.c RTOS/ARM_CM4F/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-RTOS-2f-ARM_CM4F

clean-RTOS-2f-ARM_CM4F:
	-$(RM) ./RTOS/ARM_CM4F/port.cyclo ./RTOS/ARM_CM4F/port.d ./RTOS/ARM_CM4F/port.o ./RTOS/ARM_CM4F/port.su

.PHONY: clean-RTOS-2f-ARM_CM4F

