cmake_minimum_required(VERSION 4.1)
project(32bit_cpu_emulator C)

set(CMAKE_C_STANDARD 23)

# Include the headers folder
include_directories(include)

# Add all source files from src
file(GLOB SOURCES "src/*.c")

# Create the executable
add_executable(32bit_cpu_emulator ${SOURCES}
        include/ram.h
        src/ram.c
        src/log.c
        include/log.h
        src/cpu.c
        include/cpu.h
        include/isa.h
        src/parser.c
        include/assembler.h
        src/assembler.c
        include/parser.h
        src/cpu_exec.c
        include/cpu_exec.h
        src/validation.c
)
