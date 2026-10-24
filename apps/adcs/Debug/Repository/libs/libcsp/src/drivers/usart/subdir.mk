################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/drivers/usart/usart_kiss.c 

OBJS += \
./Repository/libs/libcsp/src/drivers/usart/usart_kiss.o 

C_DEPS += \
./Repository/libs/libcsp/src/drivers/usart/usart_kiss.d 


# Each subdirectory must supply rules for building sources it contributes
Repository/libs/libcsp/src/drivers/usart/usart_kiss.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/drivers/usart/usart_kiss.c Repository/libs/libcsp/src/drivers/usart/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Repository-2f-libs-2f-libcsp-2f-src-2f-drivers-2f-usart

clean-Repository-2f-libs-2f-libcsp-2f-src-2f-drivers-2f-usart:
	-$(RM) ./Repository/libs/libcsp/src/drivers/usart/usart_kiss.cyclo ./Repository/libs/libcsp/src/drivers/usart/usart_kiss.d ./Repository/libs/libcsp/src/drivers/usart/usart_kiss.o ./Repository/libs/libcsp/src/drivers/usart/usart_kiss.su

.PHONY: clean-Repository-2f-libs-2f-libcsp-2f-src-2f-drivers-2f-usart

