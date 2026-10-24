################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_bridge.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_buffer.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_conn.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_crc32.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_debug.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_dedup.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_hex_dump.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_id.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_iflist.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_init.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_io.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_port.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_qfifo.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_route.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_service_handler.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_services.c \
/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_sfp.c 

OBJS += \
./Repository/libs/libcsp/src/csp_bridge.o \
./Repository/libs/libcsp/src/csp_buffer.o \
./Repository/libs/libcsp/src/csp_conn.o \
./Repository/libs/libcsp/src/csp_crc32.o \
./Repository/libs/libcsp/src/csp_debug.o \
./Repository/libs/libcsp/src/csp_dedup.o \
./Repository/libs/libcsp/src/csp_hex_dump.o \
./Repository/libs/libcsp/src/csp_id.o \
./Repository/libs/libcsp/src/csp_iflist.o \
./Repository/libs/libcsp/src/csp_init.o \
./Repository/libs/libcsp/src/csp_io.o \
./Repository/libs/libcsp/src/csp_port.o \
./Repository/libs/libcsp/src/csp_qfifo.o \
./Repository/libs/libcsp/src/csp_route.o \
./Repository/libs/libcsp/src/csp_service_handler.o \
./Repository/libs/libcsp/src/csp_services.o \
./Repository/libs/libcsp/src/csp_sfp.o 

C_DEPS += \
./Repository/libs/libcsp/src/csp_bridge.d \
./Repository/libs/libcsp/src/csp_buffer.d \
./Repository/libs/libcsp/src/csp_conn.d \
./Repository/libs/libcsp/src/csp_crc32.d \
./Repository/libs/libcsp/src/csp_debug.d \
./Repository/libs/libcsp/src/csp_dedup.d \
./Repository/libs/libcsp/src/csp_hex_dump.d \
./Repository/libs/libcsp/src/csp_id.d \
./Repository/libs/libcsp/src/csp_iflist.d \
./Repository/libs/libcsp/src/csp_init.d \
./Repository/libs/libcsp/src/csp_io.d \
./Repository/libs/libcsp/src/csp_port.d \
./Repository/libs/libcsp/src/csp_qfifo.d \
./Repository/libs/libcsp/src/csp_route.d \
./Repository/libs/libcsp/src/csp_service_handler.d \
./Repository/libs/libcsp/src/csp_services.d \
./Repository/libs/libcsp/src/csp_sfp.d 


# Each subdirectory must supply rules for building sources it contributes
Repository/libs/libcsp/src/csp_bridge.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_bridge.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_buffer.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_buffer.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_conn.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_conn.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_crc32.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_crc32.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_debug.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_debug.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_dedup.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_dedup.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_hex_dump.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_hex_dump.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_id.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_id.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_iflist.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_iflist.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_init.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_init.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_io.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_io.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_port.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_port.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_qfifo.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_qfifo.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_route.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_route.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_service_handler.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_service_handler.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_services.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_services.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Repository/libs/libcsp/src/csp_sfp.o: /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/libs/libcsp/src/csp_sfp.c Repository/libs/libcsp/src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Repository-2f-libs-2f-libcsp-2f-src

clean-Repository-2f-libs-2f-libcsp-2f-src:
	-$(RM) ./Repository/libs/libcsp/src/csp_bridge.cyclo ./Repository/libs/libcsp/src/csp_bridge.d ./Repository/libs/libcsp/src/csp_bridge.o ./Repository/libs/libcsp/src/csp_bridge.su ./Repository/libs/libcsp/src/csp_buffer.cyclo ./Repository/libs/libcsp/src/csp_buffer.d ./Repository/libs/libcsp/src/csp_buffer.o ./Repository/libs/libcsp/src/csp_buffer.su ./Repository/libs/libcsp/src/csp_conn.cyclo ./Repository/libs/libcsp/src/csp_conn.d ./Repository/libs/libcsp/src/csp_conn.o ./Repository/libs/libcsp/src/csp_conn.su ./Repository/libs/libcsp/src/csp_crc32.cyclo ./Repository/libs/libcsp/src/csp_crc32.d ./Repository/libs/libcsp/src/csp_crc32.o ./Repository/libs/libcsp/src/csp_crc32.su ./Repository/libs/libcsp/src/csp_debug.cyclo ./Repository/libs/libcsp/src/csp_debug.d ./Repository/libs/libcsp/src/csp_debug.o ./Repository/libs/libcsp/src/csp_debug.su ./Repository/libs/libcsp/src/csp_dedup.cyclo ./Repository/libs/libcsp/src/csp_dedup.d ./Repository/libs/libcsp/src/csp_dedup.o ./Repository/libs/libcsp/src/csp_dedup.su ./Repository/libs/libcsp/src/csp_hex_dump.cyclo ./Repository/libs/libcsp/src/csp_hex_dump.d ./Repository/libs/libcsp/src/csp_hex_dump.o ./Repository/libs/libcsp/src/csp_hex_dump.su ./Repository/libs/libcsp/src/csp_id.cyclo ./Repository/libs/libcsp/src/csp_id.d ./Repository/libs/libcsp/src/csp_id.o ./Repository/libs/libcsp/src/csp_id.su ./Repository/libs/libcsp/src/csp_iflist.cyclo ./Repository/libs/libcsp/src/csp_iflist.d ./Repository/libs/libcsp/src/csp_iflist.o ./Repository/libs/libcsp/src/csp_iflist.su ./Repository/libs/libcsp/src/csp_init.cyclo ./Repository/libs/libcsp/src/csp_init.d ./Repository/libs/libcsp/src/csp_init.o ./Repository/libs/libcsp/src/csp_init.su ./Repository/libs/libcsp/src/csp_io.cyclo ./Repository/libs/libcsp/src/csp_io.d ./Repository/libs/libcsp/src/csp_io.o ./Repository/libs/libcsp/src/csp_io.su ./Repository/libs/libcsp/src/csp_port.cyclo ./Repository/libs/libcsp/src/csp_port.d ./Repository/libs/libcsp/src/csp_port.o ./Repository/libs/libcsp/src/csp_port.su ./Repository/libs/libcsp/src/csp_qfifo.cyclo ./Repository/libs/libcsp/src/csp_qfifo.d ./Repository/libs/libcsp/src/csp_qfifo.o ./Repository/libs/libcsp/src/csp_qfifo.su ./Repository/libs/libcsp/src/csp_route.cyclo ./Repository/libs/libcsp/src/csp_route.d ./Repository/libs/libcsp/src/csp_route.o ./Repository/libs/libcsp/src/csp_route.su ./Repository/libs/libcsp/src/csp_service_handler.cyclo ./Repository/libs/libcsp/src/csp_service_handler.d ./Repository/libs/libcsp/src/csp_service_handler.o ./Repository/libs/libcsp/src/csp_service_handler.su ./Repository/libs/libcsp/src/csp_services.cyclo ./Repository/libs/libcsp/src/csp_services.d ./Repository/libs/libcsp/src/csp_services.o ./Repository/libs/libcsp/src/csp_services.su ./Repository/libs/libcsp/src/csp_sfp.cyclo ./Repository/libs/libcsp/src/csp_sfp.d ./Repository/libs/libcsp/src/csp_sfp.o ./Repository/libs/libcsp/src/csp_sfp.su

.PHONY: clean-Repository-2f-libs-2f-libcsp-2f-src

