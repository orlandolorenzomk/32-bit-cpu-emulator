#include "../include/validation.h"
#include "log.h"
#include "ram.h"

/**
 * @file validation.c
 * @brief Implementation of centralized validation helpers used by the
 * assembler and CPU execution engine.
 *
 * The functions in this module perform argument validation (bounds checks,
 * overflow detection, index validation) and emit standardized log messages
 * on error. Runtime variants accept a `CPU *` and will set
 * `cpu->running = false` when an error is detected so the caller can
 * immediately stop execution.
 */

/**
 * @brief Validate that an address literal is within RAM bounds.
 *
 * This check is intended for assembler-side validation of literal addresses
 * (for example the operand to `LOADA` or a `.org` directive). On failure
 * the function logs an error and returns false.
 *
 * @param addr Literal address to validate.
 * @return true if addr < RAM_SIZE, false otherwise.
 */
bool is_addr_literal_valid(uint32_t addr) {
    if (addr >= RAM_SIZE) {
        log_write(LOG_ERROR, "Invalid literal address: 0x%08X (must be < 0x%08X)", addr, RAM_SIZE);
        return false;
    }
    return true;
}

/**
 * @brief Assembler-side validation of a general-purpose register index.
 *
 * This helper verifies that the integer register index parsed by the
 * assembler is within [0, MAX_REGISTERS). It logs an informative error
 * message on failure and returns false. This function does not require a
 * CPU instance because it is used during assembly time.
 *
 * @param reg_index The parsed register index (may be negative on parse error).
 * @return true if valid, false otherwise.
 */
bool is_reg_index_valid_asm(int reg_index) {
    if (reg_index < 0 || reg_index >= (int)MAX_REGISTERS) {
        log_write(LOG_ERROR, "Invalid register index: %d (must be 0..%d)", reg_index, MAX_REGISTERS - 1);
        return false;
    }
    return true;
}

/**
 * @brief Assembler-side validation of an address/register index (A0..A7).
 *
 * Verifies the parsed address-register index is within
 * [0, MAX_ADDRESS_REGISTERS). Logs and returns false on invalid values.
 * This function is used by the assembler and does not modify runtime CPU
 * state.
 *
 * @param addr_index Parsed address-register index.
 * @return true if valid, false otherwise.
 */
bool is_addr_index_valid_asm(int addr_index) {
    if (addr_index < 0 || addr_index >= (int)MAX_ADDRESS_REGISTERS) {
        log_write(LOG_ERROR, "Invalid address register index: %d (must be 0..%d)", addr_index, MAX_ADDRESS_REGISTERS - 1);
        return false;
    }
    return true;
}

/**
 * @brief Validate that a memory access range [start, start+size-1] fits in RAM.
 *
 * Performs two checks:
 *  - start must be < RAM_SIZE
 *  - computing `last = start + size - 1` must not overflow and `last` must be < RAM_SIZE
 *
 * This helper is generic and intended for both assembler-time and
 * runtime checks where multi-word access needs validation.
 *
 * @param start The starting RAM index for the access.
 * @param size Number of words to access (size == 0 is considered valid).
 * @return true if the full access fits in RAM, false otherwise.
 */
bool is_memory_access_valid(uint32_t start, uint32_t size) {
    if (start >= RAM_SIZE) {
        log_write(LOG_ERROR, "Memory access start out of bounds: 0x%08X", start);
        return false;
    }
    if (size == 0) return true;

    /*
     * Compute last = start + size - 1 and check for overflow.
     * If last < start then addition wrapped around and overflow occurred.
     */
    uint32_t last = start + size - 1;
    if (last < start) {
        log_write(LOG_ERROR, "Memory access overflow: start=0x%08X size=%u", start, size);
        return false;
    }

    if (last >= RAM_SIZE) {
        log_write(LOG_ERROR, "Memory access end out of bounds: 0x%08X", last);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------------------
 * Runtime validators (accept a CPU pointer and set cpu->running=false on error)
 * --------------------------------------------------------------------------- */

/**
 * @brief Runtime validation of a general-purpose register index.
 *
 * Used by the CPU execution engine. On invalid index the function logs an
 * error, sets `cpu->running = false` to stop execution, and returns false.
 *
 * @param reg_index Register index read from the instruction stream.
 * @param cpu Pointer to CPU instance whose `running` flag will be updated on error.
 * @return true if valid, false otherwise.
 */
bool is_reg_index_valid_runtime(uint32_t reg_index, CPU *cpu) {
    if (reg_index >= MAX_REGISTERS) {
        cpu->running = false;
        log_write(LOG_ERROR, "Invalid register index %u", reg_index);
        return false;
    }
    return true;
}

/**
 * @brief Runtime validation of an address-register index (A0..A7).
 *
 * Similar to the assembler-side check but updates the runtime `cpu` state on
 * failure to stop instruction execution.
 *
 * @param addr_index Address-register index read from the instruction stream.
 * @param cpu CPU instance to update on error.
 * @return true if valid, false otherwise.
 */
bool is_addr_index_valid_runtime(uint32_t addr_index, CPU *cpu) {
    if (addr_index >= MAX_ADDRESS_REGISTERS) {
        cpu->running = false;
        log_write(LOG_ERROR, "Invalid address register index %u", addr_index);
        return false;
    }
    return true;
}

/**
 * @brief Runtime validation of a literal address operand.
 *
 * Checks that the literal address (read from the instruction stream) is
 * within RAM. On failure the function logs and stops the CPU.
 *
 * @param addr Literal address value.
 * @param cpu CPU instance to update on error.
 * @return true if the literal address is valid, false otherwise.
 */
bool is_addr_literal_valid_runtime(uint32_t addr, CPU *cpu) {
    if (addr >= RAM_SIZE) {
        cpu->running = false;
        log_write(LOG_ERROR, "Invalid literal address: 0x%08X", addr);
        return false;
    }
    return true;
}

/**
 * @brief Runtime validation of a memory access range.
 *
 * Wrapper around `is_memory_access_valid` that additionally sets
 * `cpu->running = false` when the check fails so execution halts.
 *
 * @param start Start address of the access.
 * @param size Number of words to access.
 * @param cpu CPU instance to update on error.
 * @return true if access is valid, false otherwise.
 */
bool is_memory_access_valid_runtime(uint32_t start, uint32_t size, CPU *cpu) {
    if (!is_memory_access_valid(start, size)) {
        cpu->running = false;
        return false;
    }
    return true;
}
