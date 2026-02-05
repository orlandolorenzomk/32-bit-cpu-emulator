//
// Created by dev on 2/1/26.
//

#ifndef INC_8BIT_CPU_EMULATOR_CPU_H
#define INC_8BIT_CPU_EMULATOR_CPU_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file cpu.h
 * @brief Public CPU API and state representation for the 8-bit CPU emulator.
 *
 * This header exposes the CPU state container and a small set of helper
 * functions used by the emulator. The CPU structure contains the program
 * counter, address/index registers, general-purpose registers, and a
 * running flag.
 */

/**
 * @brief Number of general-purpose 16-bit registers in the CPU.
 */
#define MAX_REGISTERS 8

/**
 * @brief Number of address/index registers.
 */
#define MAX_ADDRESS_REGISTERS 8

/**
 * @struct CPU
 * @brief CPU state container used by the emulator.
 *
 * Members:
 * - pc: Program counter. Chosen as uint32_t here to allow future expansion
 *   or to store out-of-band sentinel values; if your address space is fixed
 *   at 16 bits you may change this to uint16_t.
 * - address_registers: Array of index/address registers. Size is
 *   MAX_ADDRESS_REGISTERS.
 * - registers: General-purpose 16-bit registers used by instructions.
 * - running: Execution flag; true while the CPU is executing instructions.
 */
typedef struct {
    uint32_t pc; /**< Program counter. Interpret according to addressing model. */
    uint32_t address_registers[MAX_ADDRESS_REGISTERS]; /**< Address/index registers. */
    uint32_t registers[MAX_REGISTERS]; /**< General-purpose registers (16-bit). */
    bool zero_flag;
    bool negative_flag; /**< Negative flag set when last compare result (signed) < 0. */
    bool running; /**< True if CPU is currently running/executing. */
} CPU;

/**
 * @brief Initialize a CPU instance to a known, default state.
 *
 * This sets the program counter to zero_flag, clears all registers, and sets the
 * running flag to false.
 *
 * @param cpu Pointer to a CPU instance to initialize (must not be NULL).
 * @pre cpu != NULL
 * @post cpu->pc == 0, all registers == 0, cpu->running == false
 */
void cpu_init(CPU *cpu);

/**
 * @brief Release/reset CPU resources and return to initial state.
 *
 * Performs any necessary cleanup for a CPU instance and resets it to the
 * same state as after a call to cpu_init(). Currently this helper simply
 * calls cpu_init() (the CPU has no heap-allocated resources), but the
 * function exists to centralize cleanup if the implementation changes.
 *
 * @param cpu Pointer to the CPU instance to free/reset (may be NULL).
 */
void cpu_free(CPU *cpu);

/**
 * @brief Print CPU state for debugging purposes.
 *
 * The function should format and write the CPU state (pc, registers,
 * address registers, flags) to stdout or the project's logging facility.
 *
 * @param cpu CPU state value to print (passed by value).
 */
void cpu_print(CPU cpu);

#endif //INC_8BIT_CPU_EMULATOR_CPU_H

