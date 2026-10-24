################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_clock.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_hooks.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_queue.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_semaphore.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_system.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_time.c 

OBJS += \
./Repository/libs/libcsp/src/arch/freertos/csp_clock.o \
./Repository/libs/libcsp/src/arch/freertos/csp_hooks.o \
./Repository/libs/libcsp/src/arch/freertos/csp_queue.o \
./Repository/libs/libcsp/src/arch/freertos/csp_semaphore.o \
./Repository/libs/libcsp/src/arch/freertos/csp_system.o \
./Repository/libs/libcsp/src/arch/freertos/csp_time.o 

C_DEPS += \
./Repository/libs/libcsp/src/arch/freertos/csp_clock.d \
./Repository/libs/libcsp/src/arch/freertos/csp_hooks.d \
./Repository/libs/libcsp/src/arch/freertos/csp_queue.d \
./Repository/libs/libcsp/src/arch/freertos/csp_semaphore.d \
./Repository/libs/libcsp/src/arch/freertos/csp_system.d \
./Repository/libs/libcsp/src/arch/freertos/csp_time.d 


# Each subdirectory must supply rules for building sources it contributes
Repository/libs/libcsp/src/arch/freertos/csp_clock.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_clock.c Repository/libs/libcsp/src/arch/freertos/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/arch/freertos/csp_hooks.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_hooks.c Repository/libs/libcsp/src/arch/freertos/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/arch/freertos/csp_queue.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_queue.c Repository/libs/libcsp/src/arch/freertos/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/arch/freertos/csp_semaphore.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_semaphore.c Repository/libs/libcsp/src/arch/freertos/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/arch/freertos/csp_system.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_system.c Repository/libs/libcsp/src/arch/freertos/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/arch/freertos/csp_time.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/arch/freertos/csp_time.c Repository/libs/libcsp/src/arch/freertos/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Repository-2f-libs-2f-libcsp-2f-src-2f-arch-2f-freertos

clean-Repository-2f-libs-2f-libcsp-2f-src-2f-arch-2f-freertos:
	-$(RM) ./Repository/libs/libcsp/src/arch/freertos/csp_clock.cyclo ./Repository/libs/libcsp/src/arch/freertos/csp_clock.d ./Repository/libs/libcsp/src/arch/freertos/csp_clock.o ./Repository/libs/libcsp/src/arch/freertos/csp_clock.su ./Repository/libs/libcsp/src/arch/freertos/csp_hooks.cyclo ./Repository/libs/libcsp/src/arch/freertos/csp_hooks.d ./Repository/libs/libcsp/src/arch/freertos/csp_hooks.o ./Repository/libs/libcsp/src/arch/freertos/csp_hooks.su ./Repository/libs/libcsp/src/arch/freertos/csp_queue.cyclo ./Repository/libs/libcsp/src/arch/freertos/csp_queue.d ./Repository/libs/libcsp/src/arch/freertos/csp_queue.o ./Repository/libs/libcsp/src/arch/freertos/csp_queue.su ./Repository/libs/libcsp/src/arch/freertos/csp_semaphore.cyclo ./Repository/libs/libcsp/src/arch/freertos/csp_semaphore.d ./Repository/libs/libcsp/src/arch/freertos/csp_semaphore.o ./Repository/libs/libcsp/src/arch/freertos/csp_semaphore.su ./Repository/libs/libcsp/src/arch/freertos/csp_system.cyclo ./Repository/libs/libcsp/src/arch/freertos/csp_system.d ./Repository/libs/libcsp/src/arch/freertos/csp_system.o ./Repository/libs/libcsp/src/arch/freertos/csp_system.su ./Repository/libs/libcsp/src/arch/freertos/csp_time.cyclo ./Repository/libs/libcsp/src/arch/freertos/csp_time.d ./Repository/libs/libcsp/src/arch/freertos/csp_time.o ./Repository/libs/libcsp/src/arch/freertos/csp_time.su

.PHONY: clean-Repository-2f-libs-2f-libcsp-2f-src-2f-arch-2f-freertos

