CXX = i686-linux-gnu-g++
AS  = nasm
LD  = i686-linux-gnu-ld

CXXFLAGS = -c -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
ASFLAGS  = -f elf32
LDFLAGS  = -T linker.ld -m elf_i386

TARGET = my_kernel.bin

OBJ = boot.o kernel.o

all: $(TARGET)

boot.o: boot.asm
	$(AS) $(ASFLAGS) boot.asm -o boot.o

kernel.o: kernel.cpp
	$(CXX) $(CXXFLAGS) kernel.cpp -o kernel.o

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $(TARGET)

run: all
	qemu-system-i386 -kernel $(TARGET)

clean:
	rm -f *.o $(TARGET)