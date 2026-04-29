#ifndef I8080_H
#define I8080_H

#include <stdint.h>

/* ----------------------------------------------------------------
 * Intel 8080 CPU state
 * ---------------------------------------------------------------- */

typedef struct i8080 {
    uint16_t pc, sp;

    uint8_t a, b, c, d, e, h, l;

    struct {
        uint8_t z, s, p, cy, ac;
    } flags;

    uint8_t interrupt_enabled; /* interrupt enable */
    uint8_t halted; /* HLT state */

    uint64_t cycles;
} i8080_t;

/* ----------------------------------------------------------------
 * Memory / I/O callbacks (host must implement)
 * ---------------------------------------------------------------- */

uint8_t i8080_mem_read8(i8080_t *cpu, uint16_t address);

void i8080_mem_write8(i8080_t *cpu, uint16_t address, uint8_t value);

uint8_t i8080_io_read8(i8080_t *cpu, uint8_t port);

void i8080_io_write8(i8080_t *cpu, uint8_t port, uint8_t value);

/* ----------------------------------------------------------------
 * Register and flag helpers
 * ---------------------------------------------------------------- */

static inline uint16_t i8080_hl(const i8080_t *cpu) {
    return (uint16_t) (((uint16_t) cpu->h << 8) | cpu->l);
}

static inline uint8_t i8080_parity_even(uint8_t value) {
    value ^= value >> 4;
    value &= 0x0f;
    return (uint8_t) (((0x6996u >> value) & 1u) ^ 1u);
}

static inline void i8080_set_zsp(i8080_t *cpu, const uint8_t value) {
    cpu->flags.z = (value == 0);
    cpu->flags.s = (value >> 7) & 1u;
    cpu->flags.p = i8080_parity_even(value);
}

static inline uint8_t i8080_get_reg(const i8080_t *cpu, const uint8_t reg) {
    switch (reg & 7) {
        case 0: return cpu->b;
        case 1: return cpu->c;
        case 2: return cpu->d;
        case 3: return cpu->e;
        case 4: return cpu->h;
        case 5: return cpu->l;
        case 7: return cpu->a;
    }
    return 0;
}

static inline void i8080_set_reg(i8080_t *cpu, const uint8_t reg, const uint8_t value) {
    switch (reg & 7) {
        case 0: cpu->b = value;
            break;
        case 1: cpu->c = value;
            break;
        case 2: cpu->d = value;
            break;
        case 3: cpu->e = value;
            break;
        case 4: cpu->h = value;
            break;
        case 5: cpu->l = value;
            break;
        case 7: cpu->a = value;
            break;
    }
}

static inline uint16_t i8080_get_reg16(const i8080_t *cpu, const uint8_t register_pair) {
    switch (register_pair & 3) {
        case 0: return (uint16_t)(((uint16_t)cpu->b << 8) | cpu->c); /* BC */
        case 1: return (uint16_t)(((uint16_t)cpu->d << 8) | cpu->e); /* DE */
        case 2: return (uint16_t)(((uint16_t)cpu->h << 8) | cpu->l); /* HL */
        case 3: return cpu->sp;                                      /* SP */
    }
    return 0;
}

static inline void i8080_set_reg16(i8080_t *cpu, const uint8_t register_pair, const uint16_t value) {
    switch (register_pair & 3) {
        case 0: cpu->b = (uint8_t)(value >> 8); cpu->c = (uint8_t)value; break; /* BC */
        case 1: cpu->d = (uint8_t)(value >> 8); cpu->e = (uint8_t)value; break; /* DE */
        case 2: cpu->h = (uint8_t)(value >> 8); cpu->l = (uint8_t)value; break; /* HL */
        case 3: cpu->sp = value; break;                                    /* SP */
    }
}

/* ----------------------------------------------------------------
 * Stack and immediate-value helpers
 * ---------------------------------------------------------------- */

static inline void i8080_push16(i8080_t *cpu, const uint16_t value) {
    i8080_mem_write8(cpu, --cpu->sp, (uint8_t)(value >> 8));
    i8080_mem_write8(cpu, --cpu->sp, (uint8_t)value);
}

static inline uint16_t i8080_pop16(i8080_t *cpu) {
    const uint8_t lo = i8080_mem_read8(cpu, cpu->sp++);
    const uint8_t hi = i8080_mem_read8(cpu, cpu->sp++);
    return (uint16_t)(((uint16_t)hi << 8) | lo);
}

static inline uint16_t i8080_read16_imm(i8080_t *cpu) {
    const uint8_t lo = i8080_mem_read8(cpu, cpu->pc++);
    const uint8_t hi = i8080_mem_read8(cpu, cpu->pc++);
    return (uint16_t)(((uint16_t)hi << 8) | lo);
}


/* ----------------------------------------------------------------
 * PSW and stack register-pair helpers
 * ---------------------------------------------------------------- */

static inline uint8_t i8080_pack_flags(const i8080_t *cpu) {
    return (uint8_t)(
        0x02 |
        (cpu->flags.s  ? 0x80 : 0) |
        (cpu->flags.z  ? 0x40 : 0) |
        (cpu->flags.ac ? 0x10 : 0) |
        (cpu->flags.p  ? 0x04 : 0) |
        (cpu->flags.cy ? 0x01 : 0)
    );
}

static inline void i8080_unpack_flags(i8080_t *cpu, const uint8_t flags) {
    cpu->flags.s  = (flags >> 7) & 1u;
    cpu->flags.z  = (flags >> 6) & 1u;
    cpu->flags.ac = (flags >> 4) & 1u;
    cpu->flags.p  = (flags >> 2) & 1u;
    cpu->flags.cy = flags & 1u;
}

static inline uint16_t i8080_get_rp_stack(const i8080_t *cpu, const uint8_t rp) {
    return ((rp & 3) == 3)
        ? (uint16_t)(((uint16_t)cpu->a << 8) | i8080_pack_flags(cpu))
        : i8080_get_reg16(cpu, rp);
}

static inline void i8080_set_rp_stack(i8080_t *cpu, const uint8_t rp, const uint16_t value) {
    if ((rp & 3) == 3) {
        cpu->a = (uint8_t)(value >> 8);
        i8080_unpack_flags(cpu, (uint8_t)value);
    } else {
        i8080_set_reg16(cpu, rp, value);
    }
}

/* ----------------------------------------------------------------
 * Interrupt helper
 * ---------------------------------------------------------------- */

static inline void i8080_interrupt(i8080_t *cpu, const uint8_t rst_opcode) {
    if (!cpu->interrupt_enabled) {
        return;
    }

    cpu->interrupt_enabled = 0;
    cpu->halted = 0;

    i8080_push16(cpu, cpu->pc);
    cpu->pc = (uint16_t)((rst_opcode & 0x38u));
}

/* ----------------------------------------------------------------
 * Opcode implementation helpers
 * ---------------------------------------------------------------- */

static inline void i8080_ADD(i8080_t *cpu, const uint8_t value) {
    const uint8_t a = cpu->a;
    const uint16_t r = (uint16_t) a + value;

    cpu->a = (uint8_t) r;

    cpu->flags.cy = (r > 0xff);
    cpu->flags.ac = (((a & 0x0f) + (value & 0x0f)) > 0x0f);

    i8080_set_zsp(cpu, cpu->a);
}

static inline void i8080_SUB(i8080_t *cpu, const uint8_t value) {
    const uint8_t a = cpu->a;
    const uint16_t r = (uint16_t) a - value;

    cpu->a = (uint8_t) r;

    cpu->flags.cy = (a < value);
    cpu->flags.ac = ((a & 0x0f) >= (value & 0x0f));

    i8080_set_zsp(cpu, cpu->a);
}

static inline void i8080_ADC(i8080_t *cpu, const uint8_t value) {
    const uint8_t a = cpu->a;
    const uint8_t cy = cpu->flags.cy & 1u;
    const uint16_t r = (uint16_t) a + value + cy;

    cpu->a = (uint8_t) r;

    cpu->flags.cy = (r > 0xff);
    cpu->flags.ac = (((a & 0x0f) + (value & 0x0f) + cy) > 0x0f);

    i8080_set_zsp(cpu, cpu->a);
}

static inline void i8080_SBB(i8080_t *cpu, const uint8_t value) {
    const uint8_t a = cpu->a;
    const uint8_t cy = cpu->flags.cy & 1u;
    const uint16_t sub = (uint16_t) value + cy;
    const uint16_t r = (uint16_t) a - sub;

    cpu->a = (uint8_t) r;

    cpu->flags.cy = ((uint16_t) a < sub);
    cpu->flags.ac = ((a & 0x0f) >= ((value & 0x0f) + cy));

    i8080_set_zsp(cpu, cpu->a);
}

static inline void i8080_ANA(i8080_t *cpu, const uint8_t value) {
    cpu->flags.ac = (cpu->a | value) >> 3 & 1u;
    cpu->a = (uint8_t) (cpu->a & value);

    cpu->flags.cy = 0;

    i8080_set_zsp(cpu, cpu->a);
}

static inline void i8080_XRA(i8080_t *cpu, const uint8_t value) {
    cpu->a = (uint8_t) (cpu->a ^ value);

    cpu->flags.cy = 0;
    cpu->flags.ac = 0;

    i8080_set_zsp(cpu, cpu->a);
}

static inline void i8080_ORA(i8080_t *cpu, const uint8_t value) {
    cpu->a = (uint8_t) (cpu->a | value);

    cpu->flags.cy = 0;
    cpu->flags.ac = 0;

    i8080_set_zsp(cpu, cpu->a);
}

static inline void i8080_CMP(i8080_t *cpu, const uint8_t value) {
    const uint8_t a = cpu->a;
    const uint8_t r = (uint8_t) (a - value);

    cpu->flags.cy = (a < value);
    cpu->flags.ac = ((a & 0x0f) >= (value & 0x0f));

    i8080_set_zsp(cpu, r);
}

static inline void i8080_RST(i8080_t *cpu, const uint8_t n) {
    i8080_push16(cpu, cpu->pc);
    cpu->pc = (uint16_t)((n & 7u) * 8u);
}

static inline void i8080_alu(i8080_t *cpu, uint8_t op, uint8_t value) {
    switch (op & 7) {
        case 0: i8080_ADD(cpu, value); break;
        case 1: i8080_ADC(cpu, value); break;
        case 2: i8080_SUB(cpu, value); break;
        case 3: i8080_SBB(cpu, value); break;
        case 4: i8080_ANA(cpu, value); break;
        case 5: i8080_XRA(cpu, value); break;
        case 6: i8080_ORA(cpu, value); break;
        case 7: i8080_CMP(cpu, value); break;
    }
}

static inline uint8_t i8080_INR(i8080_t *cpu, uint8_t old) {
    const uint8_t value = old + 1;
    cpu->flags.ac = (((old & 0x0f) + 1) > 0x0f);
    i8080_set_zsp(cpu, value);
    return value;
}

static inline uint8_t i8080_DCR(i8080_t *cpu, uint8_t old) {
    const uint8_t value = old - 1;
    cpu->flags.ac = ((old & 0x0f) != 0);
    i8080_set_zsp(cpu, value);
    return value;
}

static inline void i8080_RLC(i8080_t *cpu) {
    const uint8_t cy = cpu->a >> 7;
    cpu->a = (uint8_t)((cpu->a << 1) | cy);
    cpu->flags.cy = cy;
}

static inline void i8080_RRC(i8080_t *cpu) {
    const uint8_t cy = cpu->a & 1u;
    cpu->a = (uint8_t)((cpu->a >> 1) | (cy << 7));
    cpu->flags.cy = cy;
}

static inline void i8080_RAL(i8080_t *cpu) {
    const uint8_t new_cy = cpu->a >> 7;
    cpu->a = (uint8_t)((cpu->a << 1) | cpu->flags.cy);
    cpu->flags.cy = new_cy;
}

static inline void i8080_RAR(i8080_t *cpu) {
    const uint8_t new_cy = cpu->a & 1u;
    cpu->a = (uint8_t)((cpu->a >> 1) | (cpu->flags.cy << 7));
    cpu->flags.cy = new_cy;
}

static inline void i8080_DAA(i8080_t *cpu) {
    const uint8_t a = cpu->a;
    const uint8_t cy = cpu->flags.cy;

    uint8_t da = 0;
    uint8_t new_cy = cy;

    if (cpu->flags.ac || (a & 0x0f) > 9)
        da = 0x06;

    if (cy || a > 0x99) {
        da |= 0x60;
        new_cy = 1;
    }

    cpu->a = (uint8_t)(a + da);
    cpu->flags.cy = new_cy;
    cpu->flags.ac = (((a & 0x0f) + (da & 0x0f)) > 0x0f);
    i8080_set_zsp(cpu, cpu->a);
}

static inline void i8080_DAD(i8080_t *cpu, const uint16_t value) {
    const uint32_t r = (uint32_t)i8080_hl(cpu) + value;
    cpu->h = (uint8_t)(r >> 8);
    cpu->l = (uint8_t)r;
    cpu->flags.cy = (r > 0xffffu);
}

static inline uint8_t i8080_read_r8(i8080_t *cpu, const uint8_t r) {
    return (r == 6) ? i8080_mem_read8(cpu, i8080_hl(cpu)) : i8080_get_reg(cpu, r);
}

static inline void i8080_write_r8(i8080_t *cpu, const uint8_t r, const uint8_t value) {
    if (r == 6) {
        i8080_mem_write8(cpu, i8080_hl(cpu), value);
    } else {
        i8080_set_reg(cpu, r, value);
    }
}

static inline int i8080_cond(const i8080_t *cpu, const uint8_t opcode) {
    switch ((opcode >> 3) & 7) {
        case 0: return !cpu->flags.z;  /* NZ */
        case 1: return  cpu->flags.z;  /* Z  */
        case 2: return !cpu->flags.cy; /* NC */
        case 3: return  cpu->flags.cy; /* C  */
        case 4: return !cpu->flags.p;  /* PO */
        case 5: return  cpu->flags.p;  /* PE */
        case 6: return !cpu->flags.s;  /* P  */
        case 7: return  cpu->flags.s;  /* M  */
    }
    return 0;
}

/* ----------------------------------------------------------------
 * Opcode dispatch and execution loop
 * ---------------------------------------------------------------- */

static inline int i8080_run(i8080_t *cpu, const int max_cycles) {
    int cycles = 0;

    while (cycles < max_cycles) {
        const uint8_t opcode = i8080_mem_read8(cpu, cpu->pc++);

        switch (opcode) {
            case 0x00: /* NOP */
            case 0x08: case 0x10: case 0x18: /* illegal NOP */
            case 0x20: case 0x28: case 0x30: case 0x38:
                cycles += 4;
                break;

            case 0x02: /* STAX B */
            case 0x12: /* STAX D */
            {
                const uint8_t rp = (opcode >> 4) & 1;
                i8080_mem_write8(cpu, i8080_get_reg16(cpu, rp), cpu->a);
                cycles += 7;
                break;
            }

            case 0x0a: /* LDAX B */
            case 0x1a: /* LDAX D */
            {
                const uint8_t rp = (opcode >> 4) & 1;
                cpu->a = i8080_mem_read8(cpu, i8080_get_reg16(cpu, rp));
                cycles += 7;
                break;
            }

            case 0x06: /* MVI B, d8 */
            case 0x0e: /* MVI C, d8 */
            case 0x16: /* MVI D, d8 */
            case 0x1e: /* MVI E, d8 */
            case 0x26: /* MVI H, d8 */
            case 0x2e: /* MVI L, d8 */
            case 0x36: /* MVI M, d8 */
            case 0x3e: /* MVI A, d8 */
            {
                const uint8_t dst = (opcode >> 3) & 7;
                const uint8_t value = i8080_mem_read8(cpu, cpu->pc++);

                if (dst == 6) {
                    i8080_mem_write8(cpu, i8080_hl(cpu), value);
                    cycles += 10;
                } else {
                    i8080_set_reg(cpu, dst, value);
                    cycles += 7;
                }
                break;
            }

            case 0x07: /* RLC */ i8080_RLC(cpu); cycles += 4; break;
            case 0x0f: /* RRC */ i8080_RRC(cpu); cycles += 4; break;
            case 0x17: /* RAL */ i8080_RAL(cpu); cycles += 4; break;
            case 0x1f: /* RAR */ i8080_RAR(cpu); cycles += 4; break;

            case 0x09: /* DAD B */
            case 0x19: /* DAD D */
            case 0x29: /* DAD H */
            case 0x39: /* DAD SP */
            {
                const uint8_t rp = (opcode >> 4) & 3;
                i8080_DAD(cpu, i8080_get_reg16(cpu, rp));
                cycles += 10;
                break;
            }

            case 0x22: /* SHLD a16 */
            {
                const uint16_t addr = i8080_read16_imm(cpu);
                i8080_mem_write8(cpu, addr, cpu->l);
                i8080_mem_write8(cpu, (uint16_t)(addr + 1), cpu->h);
                cycles += 16;
                break;
            }

            case 0x2a: /* LHLD a16 */
            {
                const uint16_t addr = i8080_read16_imm(cpu);
                cpu->l = i8080_mem_read8(cpu, addr);
                cpu->h = i8080_mem_read8(cpu, (uint16_t)(addr + 1));
                cycles += 16;
                break;
            }

            case 0x27: /* DAA */ i8080_DAA(cpu); cycles += 4; break;

            case 0x2f: /* CMA */ cpu->a = ~cpu->a; cycles += 4; break;

            case 0x32: /* STA a16 */
            {
                i8080_mem_write8(cpu, i8080_read16_imm(cpu), cpu->a);
                cycles += 13;
                break;
            }

            case 0x37: /* STC */ cpu->flags.cy = 1; cycles += 4; break;
            case 0x3f: /* CMC */ cpu->flags.cy ^= 1u; cycles += 4; break;

            case 0x3a: /* LDA a16 */
            {
                cpu->a = i8080_mem_read8(cpu, i8080_read16_imm(cpu));
                cycles += 13;
                break;
            }

            case 0x01: /* LXI B, d16 */
            case 0x11: /* LXI D, d16 */
            case 0x21: /* LXI H, d16 */
            case 0x31: /* LXI SP,d16 */
            {
                const uint8_t rp = (opcode >> 4) & 3;
                i8080_set_reg16(cpu, rp, i8080_read16_imm(cpu));
                cycles += 10;
                break;
            }

            case 0x03: /* INX B */
            case 0x13: /* INX D */
            case 0x23: /* INX H */
            case 0x33: /* INX SP */
            {
                const uint8_t rp = (opcode >> 4) & 3;
                i8080_set_reg16(cpu, rp, (uint16_t)(i8080_get_reg16(cpu, rp) + 1));
                cycles += 5;
                break;
            }

            case 0x0b: /* DCX B */
            case 0x1b: /* DCX D */
            case 0x2b: /* DCX H */
            case 0x3b: /* DCX SP */
            {
                const uint8_t rp = (opcode >> 4) & 3;
                i8080_set_reg16(cpu, rp, (uint16_t)(i8080_get_reg16(cpu, rp) - 1));
                cycles += 5;
                break;
            }

            case 0x04: case 0x0c: case 0x14: case 0x1c: /* INR r / INR M */
            case 0x24: case 0x2c: case 0x34: case 0x3c:
            {
                const uint8_t r = (opcode >> 3) & 7;
                const uint8_t value = i8080_INR(cpu, i8080_read_r8(cpu, r));
                i8080_write_r8(cpu, r, value);
                cycles += (r == 6) ? 10 : 5;
                break;
            }

            case 0x05: case 0x0d: case 0x15: case 0x1d: /* DCR r / DCR M */
            case 0x25: case 0x2d: case 0x35: case 0x3d:
            {
                const uint8_t r = (opcode >> 3) & 7;
                const uint8_t value = i8080_DCR(cpu, i8080_read_r8(cpu, r));
                i8080_write_r8(cpu, r, value);
                cycles += (r == 6) ? 10 : 5;
                break;
            }

            case 0x40 ... 0x7f: /* MOV r,r / MOV r,M / MOV M,r / HLT */
            {
                if (opcode == 0x76) {
                    cpu->halted = 1;
                    cycles += 7;
                    break;
                }

                const uint8_t dst = (opcode >> 3) & 7;
                const uint8_t src = opcode & 7;

                i8080_write_r8(cpu, dst, i8080_read_r8(cpu, src));

                cycles += (dst == 6 || src == 6) ? 7 : 5;
                break;
            }

            case 0x80 ... 0xbf: /* ADD/ADC/SUB/SBB/ANA/XRA/ORA/CMP r */
            {
                const uint8_t op = (opcode >> 3) & 7;
                const uint8_t src = opcode & 7;
                i8080_alu(cpu, op, i8080_read_r8(cpu, src));
                cycles += (src == 6) ? 7 : 4;
                break;
            }

            case 0xc6: case 0xce: case 0xd6: case 0xde: /* ADI/ACI/SUI/SBI */
            case 0xe6: case 0xee: case 0xf6: case 0xfe: /* ANI/XRI/ORI/CPI */
            {
                const uint8_t op = (opcode >> 3) & 7;
                i8080_alu(cpu, op, i8080_mem_read8(cpu, cpu->pc++));
                cycles += 7;
                break;
            }

            case 0xc3: /* JMP addr */
            case 0xcb: /* JMP addr (illegal alias) */
                cpu->pc = i8080_read16_imm(cpu);
                cycles += 10;
                break;

            case 0xc2: /* JNZ a16 */
            case 0xca: /* JZ  a16 */
            case 0xd2: /* JNC a16 */
            case 0xda: /* JC  a16 */
            case 0xe2: /* JPO a16 */
            case 0xea: /* JPE a16 */
            case 0xf2: /* JP  a16 */
            case 0xfa: /* JM  a16 */
            {
                const uint16_t addr = i8080_read16_imm(cpu);
                if (i8080_cond(cpu, opcode)) cpu->pc = addr;
                cycles += 10;
                break;
            }

            case 0xcd: /* CALL addr */
            case 0xdd: /* CALL addr (illegal alias) */
            case 0xed:
            case 0xfd:
            {
                const uint16_t addr = i8080_read16_imm(cpu);
                i8080_push16(cpu, cpu->pc);
                cpu->pc = addr;
                cycles += 17;
                break;
            }

            case 0xc4: /* CNZ a16 */
            case 0xcc: /* CZ  a16 */
            case 0xd4: /* CNC a16 */
            case 0xdc: /* CC  a16 */
            case 0xe4: /* CPO a16 */
            case 0xec: /* CPE a16 */
            case 0xf4: /* CP  a16 */
            case 0xfc: /* CM  a16 */
            {
                const uint16_t addr = i8080_read16_imm(cpu);
                if (i8080_cond(cpu, opcode)) {
                    i8080_push16(cpu, cpu->pc);
                    cpu->pc = addr;
                    cycles += 17;
                } else {
                    cycles += 11;
                }
                break;
            }

            case 0xc9: /* RET */
            case 0xd9: /* RET (illegal alias) */
                cpu->pc = i8080_pop16(cpu);
                cycles += 10;
                break;

            case 0xc0: /* RNZ */
            case 0xc8: /* RZ  */
            case 0xd0: /* RNC */
            case 0xd8: /* RC  */
            case 0xe0: /* RPO */
            case 0xe8: /* RPE */
            case 0xf0: /* RP  */
            case 0xf8: /* RM  */
                if (i8080_cond(cpu, opcode)) {
                    cpu->pc = i8080_pop16(cpu);
                    cycles += 11;
                } else {
                    cycles += 5;
                }
                break;

            case 0xc5: /* PUSH B */
            case 0xd5: /* PUSH D */
            case 0xe5: /* PUSH H */
            case 0xf5: /* PUSH PSW */
            {
                const uint8_t rp = (opcode >> 4) & 3;
                i8080_push16(cpu, i8080_get_rp_stack(cpu, rp));
                cycles += 11;
                break;
            }

            case 0xc1: /* POP B */
            case 0xd1: /* POP D */
            case 0xe1: /* POP H */
            case 0xf1: /* POP PSW */
            {
                const uint8_t rp = (opcode >> 4) & 3;
                i8080_set_rp_stack(cpu, rp, i8080_pop16(cpu));
                cycles += 10;
                break;
            }

            case 0xc7: /* RST 0 */
            case 0xcf: /* RST 1 */
            case 0xd7: /* RST 2 */
            case 0xdf: /* RST 3 */
            case 0xe7: /* RST 4 */
            case 0xef: /* RST 5 */
            case 0xf7: /* RST 6 */
            case 0xff: /* RST 7 */
                i8080_push16(cpu, cpu->pc);
                cpu->pc = (uint16_t)(opcode & 0x38);
                cycles += 11;
                break;

            case 0xe3: /* XTHL */
            {
                const uint8_t lo = i8080_mem_read8(cpu, cpu->sp);
                const uint8_t hi = i8080_mem_read8(cpu, (uint16_t)(cpu->sp + 1));
                i8080_mem_write8(cpu, cpu->sp, cpu->l);
                i8080_mem_write8(cpu, (uint16_t)(cpu->sp + 1), cpu->h);
                cpu->l = lo;
                cpu->h = hi;
                cycles += 18;
                break;
            }

            case 0xe9: /* PCHL */
                cpu->pc = i8080_hl(cpu);
                cycles += 5;
                break;

            case 0xeb: /* XCHG */
            {
                const uint8_t h = cpu->h, l = cpu->l;
                cpu->h = cpu->d; cpu->l = cpu->e;
                cpu->d = h;      cpu->e = l;
                cycles += 4;
                break;
            }

            case 0xf9: /* SPHL */
                cpu->sp = i8080_hl(cpu);
                cycles += 5;
                break;

            case 0xf3: /* DI */
                cpu->interrupt_enabled = 0;
                cycles += 4;
                break;

            case 0xfb: /* EI */
                cpu->interrupt_enabled = 1;
                cycles += 4;
                break;

            case 0xdb: /* IN d8 */
            {
                const uint8_t port = i8080_mem_read8(cpu, cpu->pc++);
                cpu->a = i8080_io_read8(cpu, port);
                cycles += 10;
                break;
            }

            case 0xd3: /* OUT d8 */
            {
                const uint8_t port = i8080_mem_read8(cpu, cpu->pc++);
                i8080_io_write8(cpu, port, cpu->a);
                cycles += 10;
                break;
            }
        }

        if (cpu->halted) break;
    }

    cpu->cycles += cycles;

    return cycles;
}

#endif
