//
// Created by dev on 2/3/26.
//

#ifndef INC_8BIT_CPU_EMULATOR_CPU_EXEC_H
#define INC_8BIT_CPU_EMULATOR_CPU_EXEC_H

#include "cpu.h"
#include "ram.h"
#include "assembler.h"

/**
 * @brief Execute the program loaded into RAM between start and end addresses.
 *
 * The function fetches and executes instructions from RAM starting at
 * assembly_range.start_address and stopping when pc reaches
 * assembly_range.end_address or when a fatal error/HALT occurs. The CPU
 * state (registers, pc, running flag) is updated in-place.
 *
 * @param cpu Pointer to initialized CPU state used for execution (must be non-NULL).
 * @param ram Pointer to initialized RAM containing program/data (must be non-NULL).
 * @param assembly_range Address range [start_address, end_address) describing the loaded program.
 * @return true on normal completion (HALT or reached end_address), false on execution error.
 */
bool cpu_run(CPU *cpu, RAM *ram, AssemblyRange assembly_range);

#endif //INC_8BIT_CPU_EMULATOR_CPU_EXEC_H