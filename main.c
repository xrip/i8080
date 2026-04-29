#include <stdio.h>
#include <string.h>
#include "i8080.h"

static uint8_t mem[0x10000];

uint8_t i8080_mem_read8(i8080_t *cpu, uint16_t address) {
    return mem[address];
}

void i8080_mem_write8(i8080_t *cpu, uint16_t address, uint8_t value) {
    mem[address] = value;
}

uint8_t i8080_io_read8(i8080_t *cpu, uint8_t port) {
    return 0;
}

static int g_exit_requested;

void i8080_io_write8(i8080_t *cpu, uint8_t port, uint8_t value) {
    if (port == 0) {
        g_exit_requested = 1;
        cpu->halted = 1;
    } else if (port == 1) {
        if (cpu->c == 2) {
            putchar(cpu->e);
        } else if (cpu->c == 9) {
            uint16_t addr = (uint16_t)(((uint16_t)cpu->d << 8) | cpu->e);
            for (int i = 0; i < 0x10000; ++i) {
                char ch = (char)mem[addr++];
                if (ch == '$') break;
                putchar(ch);
            }
        }
        fflush(stdout);
    }
}

#define TEST_PATH(name) I8080_TESTS_DIR "/" name

static const char *tests[] = {
    TEST_PATH("TST8080.COM"),
    TEST_PATH("8080PRE.COM"),
    TEST_PATH("CPUTEST.COM"),
    TEST_PATH("8080EXER.COM"),
    TEST_PATH("8080EXM.COM"),
};

static int run_test(const char *path, uint64_t max_cycles) {
    memset(mem, 0, sizeof(mem));

    mem[0x0000] = 0xd3; mem[0x0001] = 0x00;
    mem[0x0005] = 0xd3; mem[0x0006] = 0x01; mem[0x0007] = 0xc9;

    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("[cannot open: %s]\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    fread(mem + 0x0100, 1, (size_t)size, f);
    fclose(f);

    i8080_t cpu = {0};
    cpu.pc = 0x0100;
    cpu.sp = 0xf000;
    g_exit_requested = 0;

    while (!g_exit_requested && !cpu.halted) {
        if (max_cycles && cpu.cycles >= max_cycles) {
            printf("\n[TIMEOUT  pc=%04X cycles=%llu]\n",
                   cpu.pc, (unsigned long long)cpu.cycles);
            return -1;
        }
        i8080_run(&cpu, 100000);
    }

    printf("\n[cycles: %llu]\n", (unsigned long long)cpu.cycles);
    return 0;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        const char *name = tests[i];
        const char *slash = name;
        for (const char *p = name; *p; ++p)
            if (*p == '/' || *p == '\\') slash = p + 1;

        printf("\n=== %s ===\n", slash);
        run_test(name, 60000000000ULL);
    }

    return 0;
}