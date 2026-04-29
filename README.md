# i8080

A header-only, highly portable Intel 8080 CPU emulator written in C.

The entire emulator lives in `i8080.h` — just include it, implement four callbacks, and call `i8080_run()`.

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

/* Then initialize the CPU state and run: */

i8080_t cpu = {0};
cpu.pc = 0x0100;  // set entry point
cpu.sp = 0xf000;  // set stack pointer

while (!cpu.halted) {
    i8080_run(&cpu, 100000);  // execute up to 100000 cycles
}
```

### Interrupts

To signal an interrupt, call `i8080_interrupt()` with a single-byte RST opcode (0xC7–0xFF, bits [5:3] encode the vector):

```c
i8080_interrupt(&cpu, 0xcf);  // RST 1 — calls address 0x0008
```

Interrupts are ignored while `cpu.interrupt_enabled` is `0` (after `DI` or a previous interrupt). `EI` sets it back to `1`.

### CPU State

The `i8080_t` struct exposes everything:

| Field                | Description                       |
|----------------------|-----------------------------------|
| `pc`, `sp`           | Program counter, stack pointer    |
| `a`–`l`             | General-purpose registers         |
| `flags.z`            | Zero                              |
| `flags.s`            | Sign                              |
| `flags.p`            | Parity                            |
| `flags.cy`           | Carry                             |
| `flags.ac`           | Auxiliary carry                   |
| `interrupt_enabled`  | Interrupt flag                    |
| `halted`             | Set by `HLT`                      |
| `cycles`             | Total cycle count (`uint64_t`)    |

## Building

```sh
cmake -B build
cmake --build build
```

## Running

```sh
./build/i8080
```

The included `main.c` runs classic 8080 test ROMs (TST8080, 8080PRE, CPUTEST, 8080EXER, 8080EXM) from the `tests/` directory to verify instruction-level accuracy.
