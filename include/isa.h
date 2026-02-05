//
// Created by dev on 2/1/26.
//

#ifndef INC_8BIT_CPU_EMULATOR_ISA_H
#define INC_8BIT_CPU_EMULATOR_ISA_H

#include <stdint.h>

/**
 * @file isa.h
 * @brief Instruction Set Architecture (ISA) definitions and assembler-facing
 * documentation for the 8-bit CPU emulator.
 *
 * This header defines the numeric opcodes used by the assembler and CPU
 * emulator and documents the human-facing assembly syntax and operand
 * conventions used across the project. The examples and conventions below
 * should be used by the assembler, emulator, and any tooling that reads or
 * generates assembly for this ISA.
 *
 * Conventions used in examples below:
 *  - Registers: R(i) are general-purpose registers (R0, R1, R2, ...).
 *  - Address registers: A(i) hold memory addresses (A0, A1, ...).
 *  - Immediate values: written as plain decimal or 0x-prefixed hex (e.g. 42, 0x2A).
 *  - Memory operand syntax: (ADDR) or (A0) means "the memory at address ADDR".
 *      - (A0) means: use the address stored in register A0, then access memory
 *        at that address (i.e. dereference the register).
 *  - Instruction operand order: two-operand instructions use "DEST, SRC" order.
 *      Example: LOADI R2, #10    ; load immediate 10 into R2
 *               ADD   R2, R3    ; R2 = R2 + R3
 *               LOADM R2, (A0)  ; R2 = MEM[A0]
 *               STOREM (A0), R2 ; MEM[A0] = R2
 *
 * Addressing modes (used in examples):
 *  - Immediate: a literal value encoded in the instruction stream (e.g. LOADI)
 *  - Direct address: a numeric address encoded in the instruction (e.g. LOADA A0, 0x2000)
 *  - Indirect (register indirect): parentheses around a register (A0) means
 *    "memory at the address contained in A0".
 *
 * Notes:
 *  - Parentheses are significant: `R2` means the register value, `(A0)` means
 *    "memory at the address in A0". This distinguishes register-to-register
 *    moves from memory loads or stores.
 *  - Check the assembler/parser and CPU implementation for exact binary
 *    encoding details and any side effects (for example, which instructions
 *    set flags like zero or carry if implemented).
 */

#define MAX_LABELS 256

/**
 * @enum isa_instruction_t
 * @brief Numeric opcode values used by the assembler and CPU.
 *
 * Each enum value documents the assembly mnemonic, expected operand order,
 * a short description of the semantics, and a small example using the
 * project's assembly conventions.
 */
typedef enum {
    ISA_LOADI = 0x01,   /**< LOADI R(i), imm  - Load immediate into register. Example: LOADI R2, 10 */
    ISA_LOADA = 0x02,   /**< LOADA A(i), addr  - Load a literal address into an address register. Example: LOADA A0, 0x2002 */
    ISA_LOADM = 0x03,   /**< LOADM R(i), (addr|A(j)) - Load from memory into register. Example: LOADM R2, (A0) */
    ISA_STOREM = 0x04,  /**< STOREM (addr|A(j)), R(i) - Store register into memory. Example: STOREM (A0), R2 */
    ISA_ADD = 0x05,     /**< ADD R(i), R(j)  - Add R[j] into R[i]. Example: ADD R1, R2 */
    ISA_SUB = 0x06,     /**< SUB R(i), R(j)  - Subtract R[j] from R[i]. Example: SUB R1, R2 */
    ISA_MLP = 0x07,     /**< MLP R(i), R(j)  - Multiply R[i] by R[j] (semantics depend on implementation). Example: MLP R1, R2 */
    ISA_DIV = 0x08,     /**< DIV R(i), R(j)  - Divide R[i] by R[j] (watch for divide-by-zero_flag semantics). Example: DIV R1, R2 */
    ISA_AND = 0x09,     /**< AND R(i), R(j)  - Bitwise AND. Example: AND R1, R2 */
    ISA_OR  = 0x0A,     /**< OR  R(i), R(j)  - Bitwise OR. Example: OR R1, R2 */
    ISA_XOR = 0x0B,     /**< XOR R(i), R(j)  - Bitwise XOR. Example: XOR R1, R2 */
    ISA_JMP = 0x0C,     /**< JMP addr        - Unconditional jump: PC := addr. Example: JMP 0x0100 */
    ISA_JZ  = 0x0D,     /**< JZ addr         - Jump if zero_flag flag set (semantics: dependent on CPU flags). Example: JZ 0x0200 */
    ISA_JNZ = 0x0E,     /**< JNZ addr        - Jump if zero_flag flag not set. Example: JNZ 0x0204 */
    ISA_CMP = 0x0F,     /**< CMP R(i), R(j)  - Compare registers (signed): sets zero_flag if equal and negative_flag if R[i] < R[j]. Example: CMP R0, R1 */
    ISA_HALT = 0xFF     /**< HALT            - Stop execution. Example: HALT */
} isa_instruction_t;

/**
 * @struct MnemonicMap
 * @brief Mapping of textual mnemonic to numeric opcode used by the assembler.
 *
 * The assembler may iterate this table to translate mnemonics into opcode
 * bytes. The table is defined as static const below for use by the
 * assembler implementation in this project.
 */
typedef struct {
    const char *mnemonic; /**< ASCII mnemonic, e.g. "LOADI" */
    uint8_t opcode;       /**< Numeric opcode (isa_instruction_t) */
} MnemonicMap;

/**
 * @brief Opcode table used by the assembler to map textual mnemonics to opcodes.
 *
 * Keep this table synchronized with isa_instruction_t. It is intentionally
 * declared static here; if you need an exported version for unit tests or
 * other modules, consider adding a non-static accessor or a duplicate table
 * in the assembler translation unit.
 */
static const MnemonicMap opcode_table[] = {
    { "LOADI", ISA_LOADI },
    { "LOADA", ISA_LOADA },
    { "LOADM", ISA_LOADM },
    { "STOREM", ISA_STOREM },
    { "ADD", ISA_ADD },
    { "SUB", ISA_SUB },
    { "MLP", ISA_MLP },
    { "DIV", ISA_DIV },
    { "AND", ISA_AND },
    { "OR", ISA_OR },
    { "XOR", ISA_XOR },
    { "JMP", ISA_JMP },
    { "JZ", ISA_JZ },
    { "JNZ", ISA_JNZ },
    { "CMP", ISA_CMP},
    { "HALT", ISA_HALT }
};

/**
 * @struct Label
 * @brief Represents a textual label/symbol tracked by the assembler.
 *
 * The assembler typically maintains a list/array of Label entries while
 * processing source to resolve symbolic addresses. `name` is a NUL-terminated
 * C string up to 63 characters (64 including terminating NUL). `address`
 * stores the resolved numeric address for the label.
 */
typedef struct {
    char name[64];     /**< NUL-terminated label name (max 63 chars) */
    uint32_t address;  /**< Resolved address for the label */
} Label;

#endif //INC_8BIT_CPU_EMULATOR_ISA_H

