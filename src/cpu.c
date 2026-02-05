//
// Created by dev on 2/1/26.
//

#include "../include/cpu.h"

#include <stddef.h>

#include "log.h"

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
void cpu_init(CPU *cpu) {
    if (!cpu) {
        log_write(LOG_ERROR, "CPU initialization failed: CPU pointer is NULL");
        return;
    }

    cpu->pc = 0;
    for (size_t i = 0; i < MAX_ADDRESS_REGISTERS; i++)
        cpu->address_registers[i] = 0;
    for (size_t i = 0; i < MAX_REGISTERS; i++)
        cpu->registers[i] = 0;
    cpu->running = false;
    cpu->zero_flag = false;
    cpu->negative_flag = false;
    log_write(LOG_INFO, "CPU initialized: PC=0, all registers cleared, running=false");
}

/**
 * @brief Reset/free CPU resources and return to initial state.
 *
 * Currently, the CPU has no dynamically allocated resources, so this
 * function simply re-initializes the CPU state by calling cpu_init(). If
 * in future the CPU holds heap resources, this function should free them
 * and then call cpu_init().
 */
void cpu_free(CPU *cpu) {
    if (!cpu) {
        log_write(LOG_WARN, "cpu_free called with NULL pointer");
        return;
    }
    // If we had allocated resources they'd be released here.
    cpu_init(cpu);
    log_write(LOG_INFO, "CPU reset via cpu_free()");
}

/**
 * @brief Print CPU state for debugging purposes.
 *
 * The function should format and write the CPU state (pc, registers,
 * address registers, flags) to stdout or the project's logging facility.
 *
 * @param cpu CPU state value to print (passed by value).
 */
void cpu_print(const CPU cpu) {
    log_write(LOG_DEBUG, "CPU State:");
    log_write(LOG_DEBUG, "  PC: 0x%08X", cpu.pc);
    log_write(LOG_DEBUG, "  Address Registers:");
    for (size_t i = 0; i < MAX_ADDRESS_REGISTERS; i++) {
        log_write(LOG_DEBUG, "    AR[%zu]: 0x%08X", i, cpu.address_registers[i]);
    }
    log_write(LOG_DEBUG, "  General-Purpose Registers:");
    for (size_t i = 0; i < MAX_REGISTERS; i++) {
        log_write(LOG_DEBUG, "    R[%zu]: %u", i, cpu.registers[i]);
    }
    log_write(LOG_DEBUG, "  Zero flag: %s", cpu.zero_flag ? "true" : "false");
    log_write(LOG_DEBUG, "  Negative flag: %s", cpu.negative_flag ? "true" : "false");
    log_write(LOG_DEBUG, "  Running: %s", cpu.running ? "true" : "false");
}