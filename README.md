# ProtoKernel - Bare-Metal x86 Hobby Kernel

<p align="center">
  <img src="https://img.shields.io/badge/Architecture-x86-blue?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Language-C%2B%2B%20%7C%20NASM-green?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Environment-QEMU-orange?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Kernel-Monolithic-red?style=for-the-badge" />
</p>

---

## 📖 Overview

A lightweight, monolithic, freestanding **32-bit hobby operating system kernel** written from scratch in **C++** and **x86 Assembly (NASM)**.

This project runs entirely **without standard libraries** (`-ffreestanding`) or any underlying operating system.  
The kernel communicates directly with simulated x86 hardware using **QEMU**.

### ✨ Features

- 🧠 Bare-metal x86 boot process
- ⚡ Interrupt-driven keyboard handling
- 🪟 Dual-pane split-screen VGA terminal with independent buffers
- 🧵 Cooperative round-robin multitasking
- 💾 Custom placement memory allocator
- ⌨️ Interactive shell terminal
- 📟 VGA text-mode rendering engine
- 🔧 Freestanding cross-compiled toolchain

---

# 🗺️ Architectural & Systems Overview

---

## 1️⃣ Bootstrapping & Memory Layout

When the x86 processor boots into **32-bit Protected Mode** through a **Multiboot-compliant bootloader** (GRUB via QEMU), GRUB loads the kernel using the Multiboot specification:

```cpp
0x1BADB002
```

Once detected:

- A dedicated **16 KB kernel stack** is initialized (`ESP`)
- Control jumps into the protected C++ kernel entry:
  
```cpp
kernel_main()
```

- The linker script (`linker.ld`) forces the binary to load at:

```cpp
0x100000
```

This avoids conflicts with legacy BIOS and reserved hardware memory regions.

---

## 2️⃣ Dual-Pane VGA terminal

Instead of writing directly into VGA memory, the kernel implements a **buffered virtual windowing system**.

### VGA Text Mode Geometry

The VGA hardware text buffer exists at:

```cpp
0xB8000
```

with dimensions:

```text
80 × 25 characters
```

Each character cell consumes:

| Bytes | Purpose |
|------|------|
| 1 | ASCII Character |
| 1 | Foreground/Background Color |

---

### Virtual Back Buffers

The kernel splits the display into **two isolated panes**.

Each pane maintains:

- Independent cursor coordinates
- Dedicated 2 KB RAM back-buffer
- Independent text rendering boundaries

---

### Rendering Pipeline

A global refresh routine:

```cpp
refresh_all()
```

maps local pane coordinates into physical VGA coordinates.

---

## 3️⃣ Programmable Interrupt Controller (PIC) & Hardware I/O

Keyboard input is handled using hardware interrupts rather than polling.

### PIC Remapping

The Intel 8259 PIC normally overlaps hardware IRQs with CPU exceptions.

This kernel remaps IRQs so the keyboard interrupt becomes:

```cpp
IRQ1 → Interrupt Vector 33 (0x21)
```

using low-level I/O port communication:

```cpp
outb()
```

---

### Interrupt Descriptor Table (IDT)

The IDT contains interrupt gate descriptors pointing to assembly wrappers.

The interrupt handler flow:

1. Push CPU registers
2. Call C++ interrupt handler
3. Read keyboard scancode from:

```cpp
0x60
```

4. Send End-of-Interrupt signal:

```cpp
0x20
```

---

## 4️⃣ Custom Placement Allocation (Heap Manager)

Since the standard runtime does not exist:

```cpp
new
```

cannot function normally.

The kernel implements a custom allocator using:

```cpp
_kernel_end
```

provided by the linker.

### Allocation Strategy

- Align memory to nearest 4 KB page
- Increment placement pointer linearly
- Override global operators:

```cpp
operator new
operator new[]
```

---

## 5️⃣ Cooperative Round-Robin Multitasking

Implemented a simple cooperative scheduler where tasks voluntarily yield execution using yield().

Each task maintains:

- Its own execution stack
- Saved CPU register values
- Task metadata required for context switching
- An isolated Shell instance context (preventing command string leaks between active panes)

---

### Context Switching

Tasks voluntarily yield execution:

```cpp
yield();
```

The assembly context switcher saves:

```text
EBP
EBX
ESI
EDI
ESP
```

and restores the next task's state.

---

# 🛠️ Development Environment

Developed and tested using:

- Windows 11
- WSL2 (Ubuntu)
- NASM
- GCC cross-compiler (`i686-linux-gnu`)
- QEMU

---

# 📦 Install Required Toolchain

Inside Ubuntu terminal, run:

```bash
sudo apt update && sudo apt upgrade -y

sudo apt install \
build-essential \
nasm \
qemu-system-x86 \
gcc-i686-linux-gnu \
g++-i686-linux-gnu \
git -y
```

---

# 🚀 Build and Run the Kernel

Inside Ubuntu:

```bash
make run
```

This:

- Assembles the bootloader
- Compiles the freestanding kernel
- Links the binary
- Launches QEMU

---

# 🧹 Clean Build Files

```bash
make clean
```

---

# 🧪 Interactive Testing Guide

Once QEMU opens, click inside the emulator window to capture keyboard focus.

---

## 1️⃣ Screen Boundary Test

Type a very long sentence without pressing Enter.

### Expected Result

Text should:

- Wrap correctly at column 39
- Continue within the left pane only
- Never overflow into the second pane

✔ Confirms correct pane geometry calculations.

---

## 2️⃣ Pane Focus Toggle Test

Press:

```text
TAB
```

### Expected Result

The hardware cursor instantly jumps between panes.

✔ Verifies:

- Keyboard IRQ handling
- IDT dispatching
- VGA cursor register manipulation

**System Architecture Note:** The right pane acts strictly as a high-frequency system telemetry monitor. While you can toggle focus to it, its background execution thread executes a continuous `clear()` and repaint cycle thousands of times per second. Any character inputs typed into this side are immediately overwritten by design before the next VGA blit interval, preserving it as a read-only dashboard.

---

## 3️⃣ Cooperative Multitasking Test

Observe the right-side metrics panel while typing into the shell.

### Expected Result

- The "Execution Pulse" line shows a rotating loading wheel glyph (`/`, `-`, `\`, `|`).
- The "Cycles Processed" metric smoothly climbs up numerically.
- Keyboard typing remains responsive on the left pane while the background thread executes.

✔ Confirms stack/context switching works correctly.

---

# ⌨️ Shell Commands

## `help`

Displays all supported shell commands.

```bash
help
```

---

## `meminfo`

Displays current heap allocation pointer.

```bash
meminfo
```

---

## `clear`

Clears the active terminal pane.

```bash
clear
```

---

# 📂 Project Structure

```text
.
├── boot.asm
├── kernel.cpp
├── linker.ld
├── Makefile
└── README.md
```

---

# ⚙️ Core Technologies

| Technology | Purpose |
|---|---|
| C++ | Kernel logic |
| NASM | Low-level assembly |
| QEMU | Hardware emulation |
| GRUB | Bootloader |
| VGA Text Mode | Display rendering |
| PIC / IDT | Interrupt handling |

---

# 🚀 Future Improvements

- [ ] Paging & Virtual Memory
- [ ] Preemptive Scheduling
- [ ] Mouse Driver
- [ ] ELF Executable Loader
- [ ] Filesystem Support
- [ ] Syscall Interface
- [ ] Separate User/Kernel Mode 
- [ ] ATA Disk Driver

---

# 📸 Preview

<img width="1918" height="1018" alt="Screenshot 2026-05-24 124149" src="https://github.com/user-attachments/assets/b8dbcb5f-cc29-4467-8d23-64adea8befde" />
<img width="732" height="457" alt="Screenshot 2026-05-24 124040" src="https://github.com/user-attachments/assets/dac27341-d969-44c6-aa88-80b2a9365610" />

---


# 👨‍💻 Author

Built from scratch as a low-level systems programming and operating system architecture project.

---
