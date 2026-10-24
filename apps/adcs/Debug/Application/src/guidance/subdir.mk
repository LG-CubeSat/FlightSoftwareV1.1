################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Application/src/guidance/target_generator.c 

OBJS += \
./Application/src/guidance/target_generator.o 

C_DEPS += \
./Application/src/guidance/target_generator.d 


# Each subdirectory must supply rules for building sources it contributes
Application/src/guidance/%.o Application/src/guidance/%.su Application/src/guidance/%.cyclo: ../Application/src/guidance/%.c Application/src/guidance/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Application-2f-src-2f-guidance

clean-Application-2f-src-2f-guidance:
	-$(RM) ./Application/src/guidance/target_generator.cyclo ./Application/src/guidance/target_generator.d ./Application/src/guidance/target_generator.o ./Application/src/guidance/target_generator.su

.PHONY: clean-Application-2f-src-2f-guidance

