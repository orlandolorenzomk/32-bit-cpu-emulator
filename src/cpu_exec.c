//
// Created by dev on 2/3/26.
//

#include "../include/cpu_exec.h"

#include "log.h"
#include "isa.h"
#include "../include/validation.h"
#include "../include/assembler.h" // for OPERAND_REGISTER / OPERAND_NUMERIC

#define INVALID_REGISTER_INDEX_ERROR_MESSAGE "Invalid register index"
#define INVALID_ADDRESS_REGISTER_INDEX_ERROR_MESSAGE "Invalid address index"
#define INVALID_LITERAL_ADDRESS_ERROR_MESSAGE "Invalid literal address"

/**
 * @brief Advance the CPU program counter by a number of 32-bit words.
 *
 * The emulator stores instructions and operands as 32-bit words in RAM.
 * This helper increments the CPU's PC by `skip` words. No bounds checks
 * are performed here; callers should ensure PC remains in-range or rely
 * on memory-access helpers that validate addresses.
 *
 * @param cpu Pointer to CPU state to update.
 * @param skip Number of 32-bit words to advance the PC.
 */
static void increase_pc(CPU *cpu, const uint32_t skip) {
    cpu->pc += skip;
}

/**
 * @brief Read a 32-bit word from RAM at cpu->pc + skip.
 *
 * This helper computes the RAM index as `cpu->pc + skip`, validates that
 * the index is within `RAM_SIZE`, logs an error and returns 0 on out-of-
 * bounds access. The returned value is the 32-bit word stored in RAM.
 *
 * @param ram Pointer to the RAM object.
 * @param cpu Pointer to the CPU (used for pc).
 * @param skip Offset (in 32-bit words) from the current pc to read.
 * @return The 32-bit word at the computed RAM index, or 0 on error.
 */
static uint32_t get_value_in_ram(RAM *ram, CPU *cpu, uint32_t skip) {
    uint32_t idx = cpu->pc + skip;
    if (idx >= RAM_SIZE) {
        log_write(LOG_ERROR, "RAM access out of bounds: 0x%08X", idx);
        return 0;
    }
    return ram->cells[idx];
}

/**
 * @brief Execute LOADI instruction (LOAD immediate into register).
 *
 * Opcode layout in RAM (32-bit words):
 *   [PC]   : ISA_LOADI
 *   [PC+1] : register index (uint32)
 *   [PC+2] : immediate value (uint32)
 *
 * Semantics: cpu->registers[register_index] = value
 * Side-effects: advances PC by 3 words. Validates register index.
 *
 * @param ram RAM containing instruction operands (read-only for LOADI)
 * @param cpu CPU state to update (registers, pc)
 * @return true on success, false on error (cpu->running will be set false)
 */
static bool handle_loadi_execution(RAM *ram, CPU *cpu) {
    uint32_t register_index = get_value_in_ram(ram, cpu, 1);
    uint32_t value = get_value_in_ram(ram, cpu, 2);

    if (!is_reg_index_valid_runtime(register_index, cpu)) {
        return false;
    }

    cpu->registers[register_index] = value;
    cpu->zero_flag = (value == 0);
    increase_pc(cpu, 3);

    return true;
}

/**
 * @brief Execute LOADA instruction (load literal address into an address register).
 *
 * Opcode layout:
 *   [PC]   : ISA_LOADA
 *   [PC+1] : address-register index (uint32)
 *   [PC+2] : literal address (uint32)
 *
 * Semantics: cpu->address_registers[address_index] = address_literal
 * Validates address-register index and that the literal address fits in RAM.
 * Advances PC by 3 words.
 *
 * @param ram RAM containing the operands
 * @param cpu CPU state to update
 * @return true on success, false on error
 */
static bool handle_loada_execution(RAM *ram, CPU *cpu) {
    uint32_t address_index = get_value_in_ram(ram, cpu, 1);
    uint32_t address_literal = get_value_in_ram(ram, cpu, 2);

    if (!is_addr_index_valid_runtime(address_index, cpu))
        return false;

    if (!is_addr_literal_valid_runtime(address_literal, cpu))
        return false;

    cpu->address_registers[address_index] = address_literal;
    increase_pc(cpu, 3);

    return true;
}

/**
 * @brief Execute LOADM instruction (load from memory into register).
 *
 * Opcode layout:
 *   [PC]   : ISA_LOADM
 *   [PC+1] : destination register index (uint32)
 *   [PC+2] : addressing mode (ADDR_LITERAL or ADDR_REGISTER)
 *   [PC+3] : operand (literal address or address-register index)
 *
 * Semantics:
 *   - If mode == ADDR_LITERAL: read from RAM[operand]
 *   - If mode == ADDR_REGISTER: read from RAM[cpu->address_registers[operand]]
 * The read value is stored into cpu->registers[register_index].
 * Validates register/index/address bounds and advances PC by 4 words.
 *
 * @param ram RAM to read from
 * @param cpu CPU state to update and validate
 * @return true on success, false on error
 */
static bool handle_loadm_execution(RAM *ram, CPU *cpu) {
    uint32_t register_index = get_value_in_ram(ram, cpu, 1);
    uint32_t mode = get_value_in_ram(ram, cpu, 2);      // ADDR_LITERAL or ADDR_REGISTER
    uint32_t operand = get_value_in_ram(ram, cpu, 3);   // literal address or address-register index

    if (!is_reg_index_valid_runtime(register_index, cpu)) {
        return false;
    }

    uint32_t target_address;

    if (mode == ADDR_LITERAL) {
        if (!is_addr_literal_valid_runtime(operand, cpu)) {
            return false;
        }
        target_address = operand;
    } else {
        if (!is_addr_index_valid_runtime(operand, cpu)) {
            return false;
        }

        target_address = cpu->address_registers[operand];
    }

    if (!is_memory_access_valid_runtime(target_address, 1, cpu)) {
        return false;
    }

    uint32_t val = ram->cells[target_address];
    cpu->registers[register_index] = val;
    cpu->zero_flag = (val == 0);
    increase_pc(cpu, 4);

    return true;
}

/**
 * @brief Execute STOREM instruction (store register into memory).
 *
 * Opcode layout:
 *   [PC]   : ISA_STOREM
 *   [PC+1] : address literal or address-register index (uint32)
 *   [PC+2] : addressing mode (ADDR_LITERAL or ADDR_REGISTER)
 *   [PC+3] : source register index (uint32)
 *
 * Semantics: compute a target address (literal or from address register) and
 * store cpu->registers[register_index] into RAM[target_address]. Validates
 * indices and bounds and advances PC by 4 words.
 *
 * @param ram RAM to write into
 * @param cpu CPU state containing registers and address registers
 * @return true on success, false on error
 */
static bool handle_storem_execution(RAM *ram, CPU *cpu) {
    uint32_t address = get_value_in_ram(ram, cpu, 1);
    uint32_t mode = get_value_in_ram(ram, cpu, 2);
    uint32_t register_index = get_value_in_ram(ram, cpu, 3);

    if (!is_reg_index_valid_runtime(register_index, cpu)) {
        return false;
    }

    uint32_t target_address;
    if (mode == ADDR_LITERAL) {
        if (!is_addr_literal_valid_runtime(address, cpu)) {
            return false;
        }
        target_address = address;
    } else {
        if (!is_addr_index_valid_runtime(address, cpu)) {
            return false;
        }
        target_address = cpu->address_registers[address];
    }

    if (!is_memory_access_valid_runtime(target_address, 1, cpu)) {
        return false;
    }

    ram->cells[target_address] = (uint32_t)cpu->registers[register_index];
    increase_pc(cpu, 4);

    return true;
}

/**
 * @brief Execute ADD instruction (R[dst] += R[src]).
 *
 * Opcode layout:
 *   [PC]   : ISA_ADD
 *   [PC+1] : destination register index
 *   [PC+2] : source register index
 *
 * Performs 32-bit addition, stores result in destination register and
 * advances PC by 3 words. Registers are validated.
 *
 * @param ram Unused for ADD but kept for uniform handler signature
 * @param cpu CPU state to read/write registers
 * @return true on success, false on error
 */
static bool handle_add_execution(RAM *ram, CPU *cpu) {
    uint32_t dst_index = get_value_in_ram(ram, cpu, 1);
    uint32_t mode = get_value_in_ram(ram, cpu, 2);
    uint32_t operand = get_value_in_ram(ram, cpu, 3);

    if (!is_reg_index_valid_runtime(dst_index, cpu))
        return false;

    uint32_t src_value;
    if (mode == OPERAND_REGISTER) {
        if (!is_reg_index_valid_runtime(operand, cpu))
            return false;
        src_value = cpu->registers[operand];
    } else if (mode == OPERAND_NUMERIC) {
        src_value = operand;
    } else {
        log_write(LOG_ERROR, "Invalid operand mode %u at PC 0x%08X", mode, cpu->pc);
        return false;
    }

    uint32_t a = cpu->registers[dst_index];
    uint32_t res = a + src_value;
    cpu->registers[dst_index] = res;
    cpu->zero_flag = (res == 0);
    increase_pc(cpu, 4);
    return true;
}

/**
 * @brief Execute SUB instruction (R[dst] -= R[src]).
 *
 * Similar layout to ADD. Performs 32-bit subtraction and updates PC.
 */
static bool handle_sub_execution(RAM *ram, CPU *cpu) {
    uint32_t dst_index = get_value_in_ram(ram, cpu, 1);
    uint32_t mode = get_value_in_ram(ram, cpu, 2);
    uint32_t operand = get_value_in_ram(ram, cpu, 3);

    if (!is_reg_index_valid_runtime(dst_index, cpu))
        return false;

    uint32_t src_value;
    if (mode == OPERAND_REGISTER) {
        if (!is_reg_index_valid_runtime(operand, cpu))
            return false;
        src_value = cpu->registers[operand];
    } else if (mode == OPERAND_NUMERIC) {
        src_value = operand;
    } else {
        log_write(LOG_ERROR, "Invalid operand mode %u at PC 0x%08X", mode, cpu->pc);
        return false;
    }

    uint32_t a = cpu->registers[dst_index];
    uint32_t res = a - src_value;
    cpu->registers[dst_index] = res;
    cpu->zero_flag = (res == 0);
    increase_pc(cpu, 4);
    return true;
}

/**
 * @brief Execute MLP instruction (32x32 -> low 32 bits stored in dst).
 *
 * Performs multiplication of two 32-bit registers. The low 32 bits of the
 * 64-bit product are stored in the destination register. PC is advanced by 3.
 */
static bool handle_mlp_execution(RAM *ram, CPU *cpu) {
    uint32_t dst_index = get_value_in_ram(ram, cpu, 1);
    uint32_t mode = get_value_in_ram(ram, cpu, 2);
    uint32_t operand = get_value_in_ram(ram, cpu, 3);

    if (!is_reg_index_valid_runtime(dst_index, cpu))
        return false;

    uint32_t src_value;
    if (mode == OPERAND_REGISTER) {
        if (!is_reg_index_valid_runtime(operand, cpu))
            return false;
        src_value = cpu->registers[operand];
    } else if (mode == OPERAND_NUMERIC) {
        src_value = operand;
    } else {
        log_write(LOG_ERROR, "Invalid operand mode %u at PC 0x%08X", mode, cpu->pc);
        return false;
    }

    uint32_t a = cpu->registers[dst_index];
    uint64_t wide = (uint64_t)a * (uint64_t)src_value;
    uint32_t prod = (uint32_t)(wide & 0xFFFFFFFFu); /* low 32 bits */
    cpu->registers[dst_index] = prod;
    cpu->zero_flag = (prod == 0);
    increase_pc(cpu, 4);
    return true;
}

/**
 * @brief Execute DIV instruction (R[dst] /= R[src]).
 *
 * Performs integer division; division-by-zero_flag stops the CPU and logs an error.
 */
static bool handle_div_execution(RAM *ram, CPU *cpu) {
    uint32_t dst_index = get_value_in_ram(ram, cpu, 1);
    uint32_t mode = get_value_in_ram(ram, cpu, 2);
    uint32_t operand = get_value_in_ram(ram, cpu, 3);

    if (!is_reg_index_valid_runtime(dst_index, cpu))
        return false;

    uint32_t divisor;
    if (mode == OPERAND_REGISTER) {
        if (!is_reg_index_valid_runtime(operand, cpu))
            return false;
        divisor = cpu->registers[operand];
    } else if (mode == OPERAND_NUMERIC) {
        divisor = operand;
    } else {
        log_write(LOG_ERROR, "Invalid operand mode %u at PC 0x%08X", mode, cpu->pc);
        return false;
    }

    if (divisor == 0) {
        log_write(LOG_ERROR, "Division by zero at PC 0x%08X", cpu->pc);
        cpu->running = false;
        return false;
    }

    uint32_t dividend = cpu->registers[dst_index];
    uint32_t quotient = dividend / divisor;
    cpu->registers[dst_index] = quotient;
    cpu->zero_flag = (quotient == 0);
    increase_pc(cpu, 4);
    return true;
}

/**
 * @brief Execute AND instruction (bitwise AND: R[dst] = R[dst] & R[src]).
 */
static bool handle_and_execution(RAM *ram, CPU *cpu) {
    uint32_t dst_index = get_value_in_ram(ram, cpu, 1);
    uint32_t mode = get_value_in_ram(ram, cpu, 2);
    uint32_t operand = get_value_in_ram(ram, cpu, 3);

    if (!is_reg_index_valid_runtime(dst_index, cpu))
        return false;

    uint32_t src_value;
    if (mode == OPERAND_REGISTER) {
        if (!is_reg_index_valid_runtime(operand, cpu))
            return false;
        src_value = cpu->registers[operand];
    } else if (mode == OPERAND_NUMERIC) {
        src_value = operand;
    } else {
        log_write(LOG_ERROR, "Invalid operand mode %u at PC 0x%08X", mode, cpu->pc);
        return false;
    }

    uint32_t res = cpu->registers[dst_index] & src_value;
    cpu->registers[dst_index] = res;
    cpu->zero_flag = res == 0;
    increase_pc(cpu, 4);
    return true;
}

/**
 * @brief Execute OR instruction (bitwise OR: R[dst] = R[dst] | R[src]).
 */
static bool handle_or_execution(RAM *ram, CPU *cpu) {
    uint32_t dst_index = get_value_in_ram(ram, cpu, 1);
    uint32_t mode = get_value_in_ram(ram, cpu, 2);
    uint32_t operand = get_value_in_ram(ram, cpu, 3);

    if (!is_reg_index_valid_runtime(dst_index, cpu))
        return false;

    uint32_t src_value;
    if (mode == OPERAND_REGISTER) {
        if (!is_reg_index_valid_runtime(operand, cpu))
            return false;
        src_value = cpu->registers[operand];
    } else if (mode == OPERAND_NUMERIC) {
        src_value = operand;
    } else {
        log_write(LOG_ERROR, "Invalid operand mode %u at PC 0x%08X", mode, cpu->pc);
        return false;
    }

    uint32_t res = cpu->registers[dst_index] | src_value;
    cpu->registers[dst_index] = res;
    cpu->zero_flag = res == 0;
    increase_pc(cpu, 4);
    return true;
}

/**
 * @brief Execute XOR instruction (bitwise XOR: R[dst] = R[dst] ^ R[src]).
 */
static bool handle_xor_execution(RAM *ram, CPU *cpu) {
    uint32_t dst_index = get_value_in_ram(ram, cpu, 1);
    uint32_t mode = get_value_in_ram(ram, cpu, 2);
    uint32_t operand = get_value_in_ram(ram, cpu, 3);

    if (!is_reg_index_valid_runtime(dst_index, cpu))
        return false;

    uint32_t src_value;
    if (mode == OPERAND_REGISTER) {
        if (!is_reg_index_valid_runtime(operand, cpu))
            return false;
        src_value = cpu->registers[operand];
    } else if (mode == OPERAND_NUMERIC) {
        src_value = operand;
    } else {
        log_write(LOG_ERROR, "Invalid operand mode %u at PC 0x%08X", mode, cpu->pc);
        return false;
    }

    uint32_t res = cpu->registers[dst_index] ^ src_value;
    cpu->registers[dst_index] = res;
    cpu->zero_flag = res == 0;
    increase_pc(cpu, 4);
    return true;
}

/**
 * @brief Execute JMP instruction (unconditional jump).
 *
 * Opcode layout:
 *   [PC]   : ISA_JMP
 *   [PC+1] : target address (uint32)
 *
 * Semantics: PC := target (no automatic increment). The target is validated
 * as a literal RAM address; invalid targets halt the CPU with an error.
 */
static bool handle_jmp_execution(RAM *ram, CPU *cpu) {
    uint32_t target = get_value_in_ram(ram, cpu, 1);

    if (!is_addr_literal_valid_runtime(target, cpu)) {
        return false;
    }

    log_write(LOG_DEBUG, "JMP taken: PC 0x%08X -> 0x%08X", cpu->pc, target);
    cpu->pc = target;
    return true;
}

/**
 * @brief Execute JZ (jump if zero flag set).
 *
 * Opcode layout:
 *   [PC]   : ISA_JZ
 *   [PC+1] : target address (uint32)
 *
 * If the CPU zero flag is set the PC is assigned to the target (no increment).
 * Otherwise the PC advances by 2 words (opcode + operand).
 */
static bool handle_jz_execution(RAM *ram, CPU *cpu) {
    uint32_t target = get_value_in_ram(ram, cpu, 1);

    if (!is_addr_literal_valid_runtime(target, cpu)) {
        return false;
    }

    if (cpu->zero_flag) {
        log_write(LOG_DEBUG, "JZ taken (zero=true): PC 0x%08X -> 0x%08X", cpu->pc, target);
        cpu->pc = target;
    } else {
        log_write(LOG_DEBUG, "JZ not taken (zero=false): PC 0x%08X -> 0x%08X", cpu->pc, cpu->pc + 2);
        increase_pc(cpu, 2);
    }

    return true;
}

/**
 * @brief Execute JNZ (jump if zero flag not set).
 *
 * Opcode layout:
 *   [PC]   : ISA_JNZ
 *   [PC+1] : target address (uint32)
 *
 * If the CPU zero flag is clear the PC is assigned to the target (no increment).
 * Otherwise the PC advances by 2 words (opcode + operand).
 */
static bool handle_jnz_execution(RAM *ram, CPU *cpu) {
    uint32_t target = get_value_in_ram(ram, cpu, 1);

    if (!is_addr_literal_valid_runtime(target, cpu)) {
        return false;
    }

    if (!cpu->zero_flag) {
        log_write(LOG_DEBUG, "JNZ taken (zero=false): PC 0x%08X -> 0x%08X", cpu->pc, target);
        cpu->pc = target;
    } else {
        log_write(LOG_DEBUG, "JNZ not taken (zero=true): PC 0x%08X -> 0x%08X", cpu->pc, cpu->pc + 2);
        increase_pc(cpu, 2);
    }

    return true;
}

/**
 * @brief Execute CMP instruction (compare two registers and set flags).
 *
 * Opcode layout:
 *   [PC]   : ISA_CMP
 *   [PC+1] : register index A
 *   [PC+2] : register index B
 *
 * Semantics: compute (int32_t)R[A] - (int32_t)R[B] and set cpu->zero_flag and
 * cpu->negative_flag accordingly. No registers are modified. Advances PC by 3.
 */
static bool handle_cmp_execution(RAM *ram, CPU *cpu) {
    uint32_t a_index = get_value_in_ram(ram, cpu, 1);
    uint32_t b_index = get_value_in_ram(ram, cpu, 2);

    if (!is_reg_index_valid_runtime(a_index, cpu))
        return false;
    if (!is_reg_index_valid_runtime(b_index, cpu))
        return false;

    int32_t a = (int32_t) cpu->registers[a_index];
    int32_t b = (int32_t) cpu->registers[b_index];
    int32_t diff = a - b;

    cpu->zero_flag = (diff == 0);
    cpu->negative_flag = (diff < 0);

    increase_pc(cpu, 3);
    return true;
}

/**
 * @brief Execute instructions from RAM between assembly_range.start_address and end_address.
 *
 * The CPU fetches a 32-bit opcode at the current PC and dispatches on the
 * instruction. Operands are read using `get_value_in_ram` at subsequent
 * word offsets. The function updates `cpu->pc` as instructions are executed
 * and sets `cpu->running` to false when execution ends (HALT) or an error
 * occurs (invalid opcode, bad register index, memory fault, division by
 * zero_flag, etc.).
 *
 * @param cpu Pointer to CPU state (pc, registers, address_registers, running).
 * @param ram Pointer to RAM containing the loaded program and data.
 * @param assembly_range Start and end addresses describing the loaded program in RAM.
 */
bool cpu_run(CPU *cpu, RAM *ram, AssemblyRange assembly_range) {
    cpu->pc = assembly_range.start_address;
    cpu->running = true;

    while (cpu->running && cpu->pc != assembly_range.end_address) {
        uint32_t instruction = get_value_in_ram(ram, cpu, 0);

        switch (instruction) {
            case ISA_LOADI:
                if (!handle_loadi_execution(ram, cpu))
                    return false;
                break;

            case ISA_LOADA:
                if (!handle_loada_execution(ram, cpu))
                    return false;
                break;

            case ISA_LOADM:
                if (!handle_loadm_execution(ram, cpu))
                    return false;
                break;

            case ISA_STOREM:
                if (!handle_storem_execution(ram, cpu))
                    return false;
                break;

            case ISA_ADD:
                if (!handle_add_execution(ram, cpu))
                    return false;
                break;

            case ISA_SUB:
                if (!handle_sub_execution(ram, cpu))
                    return false;
                break;

            case ISA_MLP:
                if (!handle_mlp_execution(ram, cpu))
                    return false;
                break;

            case ISA_DIV:
                if (!handle_div_execution(ram, cpu))
                    return false;
                break;

            case ISA_AND:
                if (!handle_and_execution(ram, cpu))
                    return false;
                break;

            case ISA_OR:
                if (!handle_or_execution(ram, cpu))
                    return false;
                break;

            case ISA_XOR:
                if (!handle_xor_execution(ram, cpu))
                    return false;
                break;

            case ISA_JMP:
                if (!handle_jmp_execution(ram, cpu))
                    return false;
                break;

            case ISA_JZ:
                if (!handle_jz_execution(ram, cpu))
                    return false;
                break;

            case ISA_JNZ:
                if (!handle_jnz_execution(ram, cpu))
                    return false;
                break;

            case ISA_CMP:
                if (!handle_cmp_execution(ram, cpu))
                    return false;
                break;

            case ISA_HALT:
                cpu->running = false;
                break;
            default:
                log_write(LOG_ERROR, "Invalid instruction 0x%08X", instruction);
                cpu->running = false;
                return false;
        }
    }

    return true;
}
