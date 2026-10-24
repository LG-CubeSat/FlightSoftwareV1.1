################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Repository/shared/interfaces/subdir.mk
-include Repository/rtos/kernels/src/subdir.mk
-include Repository/libs/libcsp/src/interfaces/subdir.mk
-include Repository/libs/libcsp/src/drivers/usart/subdir.mk
-include Repository/libs/libcsp/src/crypto/subdir.mk
-include Repository/libs/libcsp/src/cmp/subdir.mk
-include Repository/libs/libcsp/src/arch/freertos/subdir.mk
-include Repository/libs/libcsp/src/subdir.mk
-include RTOS/ARM_CM4F/subdir.mk
-include Platform/Src/subdir.mk
-include Drivers/STM32G4xx_HAL_Driver/Src/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/subdir.mk
-include Application/src/tasks/subdir.mk
-include Application/src/manager/subdir.mk
-include Application/src/guidance/subdir.mk
-include Application/src/estimation/subdir.mk
-include Application/src/control/subdir.mk
-include Application/src/communication/subdir.mk
-include Application/src/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := adcs_stm32
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
adcs_stm32.elf \

MAP_FILES += \
adcs_stm32.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
adcs_stm32.list \


# All Target
all: main-build

# Main-build Target
main-build: adcs_stm32.elf secondary-outputs

# Tool invocations
adcs_stm32.elf adcs_stm32.map: $(OBJS) $(USER_OBJS) /Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/firmware/adcs_stm32/STM32G431RBTX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "adcs_stm32.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m4 -T"/Users/wesleywong/Documents/GitHub/FlightSoftwareV1.1/firmware/adcs_stm32/STM32G431RBTX_FLASH.ld" --specs=nosys.specs -Wl,-Map="adcs_stm32.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

adcs_stm32.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "adcs_stm32.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) adcs_stm32.elf adcs_stm32.list adcs_stm32.map default.size.stdout
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
