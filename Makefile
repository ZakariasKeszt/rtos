# Name of your final executable
TARGET = rtos_sim

# Toolchain definitions
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
LD = $(PREFIX)gcc
SZ = $(PREFIX)size

# --- Folders ---
APP_DIR     = app
HAL_DIR     = hal
STARTUP_DIR = startup
BUILD_DIR   = build
KERNEL_DIR  = kernel
PORT_DIR    = kernel/port/arm_cm7

# --- Sources ---
APP_SOURCES    = $(APP_DIR)/main.c
HAL_SOURCES    = $(wildcard $(HAL_DIR)/*.c)
KERNEL_SOURCES = $(wildcard $(KERNEL_DIR)/src/*.c)
PORT_SOURCES   = $(wildcard $(PORT_DIR)/*.c)

C_SOURCES = $(APP_SOURCES) $(HAL_SOURCES) $(KERNEL_SOURCES) $(PORT_SOURCES)

ASM_SOURCES = \
$(STARTUP_DIR)/stm32f756_startup.s \
$(PORT_DIR)/context_switch.S

# --- Includes ---
C_INCLUDES = \
-I$(APP_DIR) \
-I$(HAL_DIR) \
-I$(KERNEL_DIR)/inc \
-I$(PORT_DIR)

ASM_SOURCES = \
$(STARTUP_DIR)/stm32f756_startup.s \
$(PORT_DIR)/context_switch.s

# --- Compiler Flags ---
CPU = -mcpu=cortex-m7
MCU = $(CPU) -mthumb

# -O0: No optimization (crucial for debugging an OS)
# -g3: Maximum debug symbols
# -Wall: Show all warnings
CFLAGS = $(MCU) -DSTM32F756xx $(C_INCLUDES) -O0 -g3 -Wall -fdata-sections -ffunction-sections

# --- Linker Flags ---
LDSCRIPT = $(STARTUP_DIR)/stm32f756.ld
# -nostdlib: We are bare-metal, don't link standard C startup code
LDFLAGS = $(MCU) -T$(LDSCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections -nostdlib

# --- Build Rules ---
# Convert source files to object files
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
OBJECTS := $(OBJECTS:.S=.o)   # handle capital .S extension too
vpath %.s $(sort $(dir $(ASM_SOURCES)))
vpath %.S $(sort $(dir $(ASM_SOURCES)))

# Default rule: build the .elf file
all: $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(LD) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR):
	mkdir $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
