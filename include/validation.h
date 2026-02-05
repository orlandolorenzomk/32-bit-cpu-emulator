#ifndef INC_8BIT_CPU_EMULATOR_VALIDATION_H
#define INC_8BIT_CPU_EMULATOR_VALIDATION_H

#include <stdint.h>


#include "cpu.h"

/**
 * @file validation.h
 * @brief Centralized validation helpers for assembler and CPU execution.
 *
 * These helpers provide common checks (address literal range, register
 * index bounds, memory access bounds) used by multiple translation and
 * execution units. They intentionally log errors to the project's logging
 * facility so callers can rely on the function's return value to decide
 * further control flow.
 */

/**
 * @brief Validate that an address literal is a valid RAM address.
 *
 * Checks that the provided address is within [0, RAM_SIZE). Logs an error
 * if invalid and returns false. This helper is intended for use by the
 * assembler and runtime when validating literal addresses encoded in
 * assembly source or instruction streams.
 *
 * @param addr The literal address to validate.
 * @return true if the address is valid, false otherwise.
 */
bool is_addr_literal_valid(uint32_t addr);

/**
 * @brief Validate a general-purpose register index (assembler-side).
 *
 * Checks that the provided register index is in range [0, MAX_REGISTERS).
 * Logs an error if invalid and returns false. This variant is intended for
 * use by the assembler (no CPU context available).
 */
bool is_reg_index_valid_asm(int reg_index);

/**
 * @brief Validate an address-register index (assembler-side).
 *
 * Checks that the provided address-register index is in range
 * [0, MAX_ADDRESS_REGISTERS). Logs an error if invalid and returns false.
 */
bool is_addr_index_valid_asm(int addr_index);

/**
 * @brief Validate a memory access range fits inside RAM.
 *
 * Checks that the start address is < RAM_SIZE and that `start + size - 1`
 * does not overflow or exceed RAM_SIZE-1. Logs an error and returns false
 * on invalid ranges.
 */
bool is_memory_access_valid(uint32_t start, uint32_t size);

/* Runtime variants that accept CPU context and set cpu->running=false on error */
bool is_reg_index_valid_runtime(uint32_t reg_index, CPU *cpu);
bool is_addr_index_valid_runtime(uint32_t addr_index, CPU *cpu);
bool is_addr_literal_valid_runtime(uint32_t addr, CPU *cpu);
bool is_memory_access_valid_runtime(uint32_t start, uint32_t size, CPU *cpu);

#endif //INC_8BIT_CPU_EMULATOR_VALIDATION_H
