ifeq ($(shell uname -m),aarch64)
CC      = x86_64-linux-gnu-gcc
LD      = x86_64-linux-gnu-ld
OBJCOPY = x86_64-linux-gnu-objcopy
READELF = x86_64-linux-gnu-readelf
else
CC      = gcc
LD      = ld
OBJCOPY = objcopy
READELF = readelf
endif
CFLAGS  = -Os -fomit-frame-pointer -fno-stack-protector -ffreestanding \
	-nostdlib -fPIE -fno-plt -fcf-protection=none -mno-sse -mno-sse2 -m64 \
	-ffunction-sections -fdata-sections -DFW_1160=1 -DDISPLAY_89=1 \
	-I. -I../shellcode_kernel -I../shellcode_kernel_1160
LDFLAGS = -pie -T ../shellcode_kernel/linker.ld \
	-Wl,--gc-sections,--defsym,SHELLCODE_LIMIT=0x8000
TARGET  = shellcode_kernel_89.elf
TEXT_BIN = shellcode_kernel_89.bin
dump = shellcode_kernel_89.h
SRC = main.c boot_linux.c utils.c
OBJ = $(SRC:.c=.o) display_ops_89.o
HEADERS = ../shellcode_kernel/main.h ../shellcode_kernel/boot_linux.h \
	../shellcode_kernel/utils.h ../shellcode_kernel/shellcode_kernel_args.h \
	../include/config.h ../include/linux.h ../shellcode_hv/shellcode_hv.h \
	../shellcode_kernel_1160/display_ops_1160.h \
	../shellcode_kernel_1160/shellcode_kernel_args_validate.h
all: $(dump)
$(TARGET): $(OBJ) ../shellcode_kernel/linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $(TARGET)
	$(READELF) -h $(TARGET) | grep -q 'Type:[[:space:]]*DYN'
	$(READELF) -r $(TARGET) | grep -q 'There are no relocations in this file'
$(SRC:.c=.o): %.o: ../shellcode_kernel/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@
display_ops_89.o: ../shellcode_kernel_1160/display_ops_1160.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@
$(TEXT_BIN): $(TARGET)
	$(OBJCOPY) -O binary -j .shell_code $(TARGET) $(TEXT_BIN)
	# the args marker must be the only one and inside the first 71 bytes
	test "$$(cat $(TEXT_BIN) | xxd -p -c1 | tr '\n' ' ' | grep -o 'aa 11 aa 11 aa 11 aa 11' | grep -c '')" -eq 1
	test "$$(head -c 71 $(TEXT_BIN) | xxd -p -c1 | tr '\n' ' ' | grep -o 'aa 11 aa 11 aa 11 aa 11' | grep -c '')" -eq 1
$(dump): $(TEXT_BIN)
	xxd -i $(TEXT_BIN) > $(dump)
clean:
	rm -f $(OBJ) $(TARGET) $(TEXT_BIN) $(dump)
.PHONY: all clean
.DELETE_ON_ERROR:
