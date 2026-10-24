################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/rtos/kernels/src/list.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/rtos/kernels/src/queue.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/rtos/kernels/src/tasks.c 

OBJS += \
./Repository/rtos/kernels/src/list.o \
./Repository/rtos/kernels/src/queue.o \
./Repository/rtos/kernels/src/tasks.o 

C_DEPS += \
./Repository/rtos/kernels/src/list.d \
./Repository/rtos/kernels/src/queue.d \
./Repository/rtos/kernels/src/tasks.d 


# Each subdirectory must supply rules for building sources it contributes
Repository/rtos/kernels/src/list.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/rtos/kernels/src/list.c Repository/rtos/kernels/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/rtos/kernels/src/queue.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/rtos/kernels/src/queue.c Repository/rtos/kernels/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/rtos/kernels/src/tasks.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/rtos/kernels/src/tasks.c Repository/rtos/kernels/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Repository-2f-rtos-2f-kernels-2f-src

clean-Repository-2f-rtos-2f-kernels-2f-src:
	-$(RM) ./Repository/rtos/kernels/src/list.cyclo ./Repository/rtos/kernels/src/list.d ./Repository/rtos/kernels/src/list.o ./Repository/rtos/kernels/src/list.su ./Repository/rtos/kernels/src/queue.cyclo ./Repository/rtos/kernels/src/queue.d ./Repository/rtos/kernels/src/queue.o ./Repository/rtos/kernels/src/queue.su ./Repository/rtos/kernels/src/tasks.cyclo ./Repository/rtos/kernels/src/tasks.d ./Repository/rtos/kernels/src/tasks.o ./Repository/rtos/kernels/src/tasks.su

.PHONY: clean-Repository-2f-rtos-2f-kernels-2f-src

