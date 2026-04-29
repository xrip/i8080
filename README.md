# i8080

A header-only, portable Intel 8080 CPU emulator in C.

Everything is in `i8080.h`. Include it, implement four callbacks, call `i8080_run()`.

## Usage

```c
#include "i8080.h"

/* You must implement these four functions: */

uint8_t i8080_mem_read8(i8080_t *cpu, uint16_t address) {
    // Return the byte at `address` from your memory array.
}

void i8080_mem_write8(i8080_t *cpu, uint16_t address, uint8_t value) {
    // Write `value` to `address` in your memory array.
}

uint8_t i8080_io_read8(i8080_t *cpu, uint8_t port) {
    // Read a byte from I/O `port`. Return 0 if unused.
}

void i8080_io_write8(i8080_t *cpu, uint8_t port, uint8_t value) {
    // Write `value` to I/O `port`.
}

/* Then initialize and run: */

i8080_t cpu = {0};
cpu.pc = 0x0100;  // entry point
cpu.sp = 0xf000;  // stack pointer

while (!cpu.halted) {
    i8080_run(&cpu, 100000);  // run up to 100000 cycles
}
```

### Interrupts

To signal an interrupt, call `i8080_interrupt()` with an RST opcode (0xC7–0xFF, bits [5:3] encode the vector):

```c
i8080_interrupt(&cpu, 0xcf);  // RST 1, calls 0x0008
```

Interrupts are ignored while `cpu.interrupt_enabled` is `0` (after `DI` or a previous interrupt). `EI` sets it to `1`.

### CPU state

All registers and flags are directly accessible on `i8080_t`:

| Field                | Description                    |
|----------------------|--------------------------------|
| `pc`, `sp`           | Program counter, stack pointer |
| `a`–`l`             | General-purpose registers      |
| `flags.z`            | Zero                           |
| `flags.s`            | Sign                           |
| `flags.p`            | Parity                         |
| `flags.cy`           | Carry                          |
| `flags.ac`           | Auxiliary carry                |
| `interrupt_enabled`  | Interrupt flag                 |
| `halted`             | Set by `HLT`                   |
| `cycles`             | Total cycle count (`uint64_t`) |

## Building

```sh
cmake -B build
cmake --build build
```

## Running

```sh
./build/i8080
```

`main.c` runs the 8080 test ROMs from `tests/` (TST8080, 8080PRE, CPUTEST, 8080EXER, 8080EXM) to verify instruction-level accuracy.
