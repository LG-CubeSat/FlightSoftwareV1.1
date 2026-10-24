################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Application/src/control/actuator_allocator.c \
../Application/src/control/bdot.c \
../Application/src/control/control_math.c \
../Application/src/control/controller.c \
../Application/src/control/slew_controller.c \
../Application/src/control/sun_pointing.c 

OBJS += \
./Application/src/control/actuator_allocator.o \
./Application/src/control/bdot.o \
./Application/src/control/control_math.o \
./Application/src/control/controller.o \
./Application/src/control/slew_controller.o \
./Application/src/control/sun_pointing.o 

C_DEPS += \
./Application/src/control/actuator_allocator.d \
./Application/src/control/bdot.d \
./Application/src/control/control_math.d \
./Application/src/control/controller.d \
./Application/src/control/slew_controller.d \
./Application/src/control/sun_pointing.d 


# Each subdirectory must supply rules for building sources it contributes
Application/src/control/%.o Application/src/control/%.su Application/src/control/%.cyclo: ../Application/src/control/%.c Application/src/control/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G431xx -DADCS_STM32 -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I../Platform/Inc -I../Application/include -I../RTOS -I../RTOS/ARM_CM4F -I../../../rtos/kernels/include -I../../../shared/interfaces -I../../../shared/csp -I../../../libs/libcsp/include -I../../../libs/libcsp/src -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Application-2f-src-2f-control

clean-Application-2f-src-2f-control:
	-$(RM) ./Application/src/control/actuator_allocator.cyclo ./Application/src/control/actuator_allocator.d ./Application/src/control/actuator_allocator.o ./Application/src/control/actuator_allocator.su ./Application/src/control/bdot.cyclo ./Application/src/control/bdot.d ./Application/src/control/bdot.o ./Application/src/control/bdot.su ./Application/src/control/control_math.cyclo ./Application/src/control/control_math.d ./Application/src/control/control_math.o ./Application/src/control/control_math.su ./Application/src/control/controller.cyclo ./Application/src/control/controller.d ./Application/src/control/controller.o ./Application/src/control/controller.su ./Application/src/control/slew_controller.cyclo ./Application/src/control/slew_controller.d ./Application/src/control/slew_controller.o ./Application/src/control/slew_controller.su ./Application/src/control/sun_pointing.cyclo ./Application/src/control/sun_pointing.d ./Application/src/control/sun_pointing.o ./Application/src/control/sun_pointing.su

.PHONY: clean-Application-2f-src-2f-control

