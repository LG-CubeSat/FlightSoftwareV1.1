################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_clock.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_dispatch.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_ident.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_if_stats.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_mem.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_peek_poke.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_route.c 

OBJS += \
./Repository/libs/libcsp/src/cmp/csp_cmp_clock.o \
./Repository/libs/libcsp/src/cmp/csp_cmp_dispatch.o \
./Repository/libs/libcsp/src/cmp/csp_cmp_ident.o \
./Repository/libs/libcsp/src/cmp/csp_cmp_if_stats.o \
./Repository/libs/libcsp/src/cmp/csp_cmp_mem.o \
./Repository/libs/libcsp/src/cmp/csp_cmp_peek_poke.o \
./Repository/libs/libcsp/src/cmp/csp_cmp_route.o 

C_DEPS += \
./Repository/libs/libcsp/src/cmp/csp_cmp_clock.d \
./Repository/libs/libcsp/src/cmp/csp_cmp_dispatch.d \
./Repository/libs/libcsp/src/cmp/csp_cmp_ident.d \
./Repository/libs/libcsp/src/cmp/csp_cmp_if_stats.d \
./Repository/libs/libcsp/src/cmp/csp_cmp_mem.d \
./Repository/libs/libcsp/src/cmp/csp_cmp_peek_poke.d \
./Repository/libs/libcsp/src/cmp/csp_cmp_route.d 


# Each subdirectory must supply rules for building sources it contributes
Repository/libs/libcsp/src/cmp/csp_cmp_clock.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_clock.c Repository/libs/libcsp/src/cmp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/cmp/csp_cmp_dispatch.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_dispatch.c Repository/libs/libcsp/src/cmp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/cmp/csp_cmp_ident.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_ident.c Repository/libs/libcsp/src/cmp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/cmp/csp_cmp_if_stats.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_if_stats.c Repository/libs/libcsp/src/cmp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/cmp/csp_cmp_mem.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_mem.c Repository/libs/libcsp/src/cmp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/cmp/csp_cmp_peek_poke.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_peek_poke.c Repository/libs/libcsp/src/cmp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/cmp/csp_cmp_route.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/cmp/csp_cmp_route.c Repository/libs/libcsp/src/cmp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Repository-2f-libs-2f-libcsp-2f-src-2f-cmp

clean-Repository-2f-libs-2f-libcsp-2f-src-2f-cmp:
	-$(RM) ./Repository/libs/libcsp/src/cmp/csp_cmp_clock.cyclo ./Repository/libs/libcsp/src/cmp/csp_cmp_clock.d ./Repository/libs/libcsp/src/cmp/csp_cmp_clock.o ./Repository/libs/libcsp/src/cmp/csp_cmp_clock.su ./Repository/libs/libcsp/src/cmp/csp_cmp_dispatch.cyclo ./Repository/libs/libcsp/src/cmp/csp_cmp_dispatch.d ./Repository/libs/libcsp/src/cmp/csp_cmp_dispatch.o ./Repository/libs/libcsp/src/cmp/csp_cmp_dispatch.su ./Repository/libs/libcsp/src/cmp/csp_cmp_ident.cyclo ./Repository/libs/libcsp/src/cmp/csp_cmp_ident.d ./Repository/libs/libcsp/src/cmp/csp_cmp_ident.o ./Repository/libs/libcsp/src/cmp/csp_cmp_ident.su ./Repository/libs/libcsp/src/cmp/csp_cmp_if_stats.cyclo ./Repository/libs/libcsp/src/cmp/csp_cmp_if_stats.d ./Repository/libs/libcsp/src/cmp/csp_cmp_if_stats.o ./Repository/libs/libcsp/src/cmp/csp_cmp_if_stats.su ./Repository/libs/libcsp/src/cmp/csp_cmp_mem.cyclo ./Repository/libs/libcsp/src/cmp/csp_cmp_mem.d ./Repository/libs/libcsp/src/cmp/csp_cmp_mem.o ./Repository/libs/libcsp/src/cmp/csp_cmp_mem.su ./Repository/libs/libcsp/src/cmp/csp_cmp_peek_poke.cyclo ./Repository/libs/libcsp/src/cmp/csp_cmp_peek_poke.d ./Repository/libs/libcsp/src/cmp/csp_cmp_peek_poke.o ./Repository/libs/libcsp/src/cmp/csp_cmp_peek_poke.su ./Repository/libs/libcsp/src/cmp/csp_cmp_route.cyclo ./Repository/libs/libcsp/src/cmp/csp_cmp_route.d ./Repository/libs/libcsp/src/cmp/csp_cmp_route.o ./Repository/libs/libcsp/src/cmp/csp_cmp_route.su

.PHONY: clean-Repository-2f-libs-2f-libcsp-2f-src-2f-cmp

