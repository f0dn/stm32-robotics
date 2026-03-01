.PHONY: Core/Src/main.c

build: Core/Src/main.c
	@cmake --preset Debug
	@cmake --build --preset Debug
	@arm-none-eabi-objcopy -O binary build/Debug/stm.elf build/Debug/stm.bin

run: build
	@st-flash write build/Debug/stm.bin 0x08000000
	@st-flash reset

