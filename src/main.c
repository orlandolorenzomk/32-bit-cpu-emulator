#include <stdio.h>

#include "cpu.h"
#include "ram.h"
#include "assembler.h"
#include "cpu_exec.h"

/**
 * @brief Simple program runner for the CPU emulator.
 *
 * This small CLI driver initializes RAM and CPU, assembles a hard-coded
 * input file into RAM, prints the assembled range and a hex dump of the
 * emitted words, executes the loaded program and then prints the final
 * CPU state. It also clears the assembled region before exiting.
 *
 * @return exit code 0 on success.
 */
int main(void) {

    printf("=== CPU Emulator Starting ===\n");
    printf("Hello, World!\n\n");

    printf("Step 1: Initializing RAM...\n");
    RAM ram;
    ram_init(&ram);
    printf("RAM initialized successfully (size: %u cells).\n\n", RAM_SIZE);

    printf("Step 2: Initializing CPU...\n");
    CPU cpu;
    cpu_init(&cpu);
    printf("CPU initialized successfully.\n");
    printf("Initial CPU state:\n");
    cpu_print(cpu);
    printf("\n");

    const char *input_file = "/home/dev/CLionProjects/32bit-cpu-emulator/asm-programs/add.asm";
    printf("Step 3: Assembling program from file: %s\n", input_file);
    AssemblyRange assembly_range = assemble(&ram, &cpu, input_file);
    if (assembly_range.error) {
        printf("ERROR: Assembly failed. Exiting.\n");
        ram_free(&ram, 0, 0); // Free any partial allocation
        return 1;
    }
    printf("Assembly completed successfully.\n");
    printf("Assembled program range: start=%u, end=%u\n", assembly_range.start_address, assembly_range.end_address);
    printf("RAM dump of assembled program:\n");
    uint32_t start = assembly_range.start_address;
    uint32_t end = assembly_range.end_address;
    for (size_t i = start; i < end; i++) {
        printf("RAM[%zu] = 0x%04X\n", i, (unsigned)ram.cells[i]);
    }
    printf("\n");

    printf("Step 4: Executing the assembled program...\n");
    cpu_run(&cpu, &ram, assembly_range);
    printf("Program execution completed.\n");
    printf("Final CPU state:\n");
    cpu_print(cpu);
    printf("\n");

    printf("Step 5: Cleaning up RAM...\n");
    ram_free(&ram, start, end);
    printf("RAM freed successfully.\n");

    printf("=== Emulation Complete ===\n");
    return 0;
}