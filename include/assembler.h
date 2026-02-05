//
// Created by dev on 2/2/26.
//

#ifndef INC_8BIT_CPU_EMULATOR_ASSEMBLER_H
#define INC_8BIT_CPU_EMULATOR_ASSEMBLER_H

#include <stdint.h>

#include "cpu.h"
#include "ram.h"

/**
 * @brief Range of addresses produced by an assembly operation.
 *
 * start_address and end_address are inclusive bounds of the memory region
 * where assembled code/data was written. The `error` flag is set to true
 * if assembly failed and the range should be considered invalid.
 */
typedef struct {
    uint32_t start_address; /**< Inclusive start address of assembled output */
    uint32_t end_address;   /**< Inclusive end address of assembled output */
    bool error;             /**< True if assembly failed */
} AssemblyRange;

/**
 * Addressing modes for memory operands.
 *
 * ADDR_REGISTER:
 *   Operand refers to an address register (A0, A1, ...).
 *
 * ADDR_LITERAL:
 *   Operand is an absolute memory address literal (e.g. (0x2000)).
 *
 * This value is emitted into the instruction stream so the CPU
 * can interpret the following operand correctly at execution time.
 */
typedef enum {
    ADDR_REGISTER = 0,
    ADDR_LITERAL  = 1
} AddrMode;

/**
 * @enum OperandType
 * @brief Classify an operand as a register or a numeric literal.
 */
typedef enum {
    OPERAND_REGISTER = 0, /**< Operand is a register (e.g., R0) */
    OPERAND_NUMERIC  = 1  /**< Operand is a numeric literal (e.g., 0x2000 or immediate) */
} OperandType;


/**
 * @brief Assemble an input file into RAM.
 *
 * Reads the assembly source at `file_path`, assembles it, and writes the
 * resulting machine code into `ram`. The `cpu` argument provides CPU
 * context (for example, initial program counter or addressing modes) and
 * may be used or updated by the assembler depending on implementation.
 *
 * @param ram Pointer to an initialized RAM instance where code will be written.
 * @param cpu Pointer to a CPU instance used for assembly context (may be NULL
 *            if the assembler does not require CPU context).
 * @param file_path Path to the assembly source file to process (NUL-terminated).
 * @return AssemblyRange describing the range of addresses written on success;
 *         if assembly fails the returned AssemblyRange has `error == true`.
 */
AssemblyRange assemble(RAM *ram, CPU *cpu, const char *file_path);

#endif //INC_8BIT_CPU_EMULATOR_ASSEMBLER_H
