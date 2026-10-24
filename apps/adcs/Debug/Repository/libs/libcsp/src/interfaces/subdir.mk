################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_can.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_can_pbuf.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_eth.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_eth_pbuf.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_i2c.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_kiss.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_lo.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_tun.c 

OBJS += \
./Repository/libs/libcsp/src/interfaces/csp_if_can.o \
./Repository/libs/libcsp/src/interfaces/csp_if_can_pbuf.o \
./Repository/libs/libcsp/src/interfaces/csp_if_eth.o \
./Repository/libs/libcsp/src/interfaces/csp_if_eth_pbuf.o \
./Repository/libs/libcsp/src/interfaces/csp_if_i2c.o \
./Repository/libs/libcsp/src/interfaces/csp_if_kiss.o \
./Repository/libs/libcsp/src/interfaces/csp_if_lo.o \
./Repository/libs/libcsp/src/interfaces/csp_if_tun.o 

C_DEPS += \
./Repository/libs/libcsp/src/interfaces/csp_if_can.d \
./Repository/libs/libcsp/src/interfaces/csp_if_can_pbuf.d \
./Repository/libs/libcsp/src/interfaces/csp_if_eth.d \
./Repository/libs/libcsp/src/interfaces/csp_if_eth_pbuf.d \
./Repository/libs/libcsp/src/interfaces/csp_if_i2c.d \
./Repository/libs/libcsp/src/interfaces/csp_if_kiss.d \
./Repository/libs/libcsp/src/interfaces/csp_if_lo.d \
./Repository/libs/libcsp/src/interfaces/csp_if_tun.d 


# Each subdirectory must supply rules for building sources it contributes
Repository/libs/libcsp/src/interfaces/csp_if_can.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_can.c Repository/libs/libcsp/src/interfaces/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/interfaces/csp_if_can_pbuf.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_can_pbuf.c Repository/libs/libcsp/src/interfaces/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/interfaces/csp_if_eth.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_eth.c Repository/libs/libcsp/src/interfaces/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/interfaces/csp_if_eth_pbuf.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_eth_pbuf.c Repository/libs/libcsp/src/interfaces/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/interfaces/csp_if_i2c.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_i2c.c Repository/libs/libcsp/src/interfaces/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/interfaces/csp_if_kiss.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_kiss.c Repository/libs/libcsp/src/interfaces/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/interfaces/csp_if_lo.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_lo.c Repository/libs/libcsp/src/interfaces/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/interfaces/csp_if_tun.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/interfaces/csp_if_tun.c Repository/libs/libcsp/src/interfaces/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Repository-2f-libs-2f-libcsp-2f-src-2f-interfaces

clean-Repository-2f-libs-2f-libcsp-2f-src-2f-interfaces:
	-$(RM) ./Repository/libs/libcsp/src/interfaces/csp_if_can.cyclo ./Repository/libs/libcsp/src/interfaces/csp_if_can.d ./Repository/libs/libcsp/src/interfaces/csp_if_can.o ./Repository/libs/libcsp/src/interfaces/csp_if_can.su ./Repository/libs/libcsp/src/interfaces/csp_if_can_pbuf.cyclo ./Repository/libs/libcsp/src/interfaces/csp_if_can_pbuf.d ./Repository/libs/libcsp/src/interfaces/csp_if_can_pbuf.o ./Repository/libs/libcsp/src/interfaces/csp_if_can_pbuf.su ./Repository/libs/libcsp/src/interfaces/csp_if_eth.cyclo ./Repository/libs/libcsp/src/interfaces/csp_if_eth.d ./Repository/libs/libcsp/src/interfaces/csp_if_eth.o ./Repository/libs/libcsp/src/interfaces/csp_if_eth.su ./Repository/libs/libcsp/src/interfaces/csp_if_eth_pbuf.cyclo ./Repository/libs/libcsp/src/interfaces/csp_if_eth_pbuf.d ./Repository/libs/libcsp/src/interfaces/csp_if_eth_pbuf.o ./Repository/libs/libcsp/src/interfaces/csp_if_eth_pbuf.su ./Repository/libs/libcsp/src/interfaces/csp_if_i2c.cyclo ./Repository/libs/libcsp/src/interfaces/csp_if_i2c.d ./Repository/libs/libcsp/src/interfaces/csp_if_i2c.o ./Repository/libs/libcsp/src/interfaces/csp_if_i2c.su ./Repository/libs/libcsp/src/interfaces/csp_if_kiss.cyclo ./Repository/libs/libcsp/src/interfaces/csp_if_kiss.d ./Repository/libs/libcsp/src/interfaces/csp_if_kiss.o ./Repository/libs/libcsp/src/interfaces/csp_if_kiss.su ./Repository/libs/libcsp/src/interfaces/csp_if_lo.cyclo ./Repository/libs/libcsp/src/interfaces/csp_if_lo.d ./Repository/libs/libcsp/src/interfaces/csp_if_lo.o ./Repository/libs/libcsp/src/interfaces/csp_if_lo.su ./Repository/libs/libcsp/src/interfaces/csp_if_tun.cyclo ./Repository/libs/libcsp/src/interfaces/csp_if_tun.d ./Repository/libs/libcsp/src/interfaces/csp_if_tun.o ./Repository/libs/libcsp/src/interfaces/csp_if_tun.su

.PHONY: clean-Repository-2f-libs-2f-libcsp-2f-src-2f-interfaces

