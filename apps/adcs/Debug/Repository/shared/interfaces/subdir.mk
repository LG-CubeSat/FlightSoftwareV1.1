################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/shared/interfaces/frame.c 

OBJS += \
./Repository/shared/interfaces/frame.o 

C_DEPS += \
./Repository/shared/interfaces/frame.d 


# Each subdirectory must supply rules for building sources it contributes
Repository/shared/interfaces/frame.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/shared/interfaces/frame.c Repository/shared/interfaces/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Repository-2f-shared-2f-interfaces

clean-Repository-2f-shared-2f-interfaces:
	-$(RM) ./Repository/shared/interfaces/frame.cyclo ./Repository/shared/interfaces/frame.d ./Repository/shared/interfaces/frame.o ./Repository/shared/interfaces/frame.su

.PHONY: clean-Repository-2f-shared-2f-interfaces

