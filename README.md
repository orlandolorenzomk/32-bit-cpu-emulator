# 32-bit CPU Emulator — simple, honest documentation

This repository contains a compact 32-bit CPU emulator and a tiny assembler. It's a small educational project: readable C code, a straightforward assembler, and a few example assembly programs in `asm-programs/` so you can play with writing and running assembly.

What this repo is

- A lightweight CPU emulator written in C.
- A tiny assembler that converts human-readable assembly into the emulator's memory layout.
- Example programs in `asm-programs/` (addition, loops, branching, etc.).

Why you might care

- Learn how an assembler and CPU interpreter interact.
- Experiment with a small, well-contained ISA (instruction set) and see the machine state change.
- Use as a starting point for more features: I/O, interrupts, better tooling, or a nicer CLI.

Quick status

- Code: plain C (no external dependencies beyond a C toolchain and CMake).
- Build system: CMake (project already includes `CMakeLists.txt`). The debug build output from CLion is in `cmake-build-debug/`.
- Entry point: the binary prints steps, assembles a hard-coded file from `asm-programs/`, runs it, and dumps CPU state.

Quick start (recommended)

Open a terminal at the repository root and run:

```sh
# create an out-of-source build and compile
cmake -S . -B build
cmake --build build -- -j

# run the built binary
./build/32bit_cpu_emulator
```

Notes:
- The repository also contains a CLion debug build output in `cmake-build-debug/` — you can run `cmake-build-debug/32bit_cpu_emulator` directly if you prefer.
- The current `main` is a simple driver and uses a hard-coded assembly file path (`asm-programs/add.asm`). If you want to run a different program, either change `src/main.c` or build a small wrapper.

Where the code lives

- Core headers: `include/` (CPU, RAM, assembler, ISA, parser, validation, etc.)
- Source files: `src/`
- Example assembly programs: `asm-programs/`
- Build artifacts (CLion debug): `cmake-build-debug/`

Supported instruction set (short reference)

This emulator implements a compact ISA. Below is the human-friendly mnemonic list and what they do (exact encodings are in `include/isa.h`):

- LOADI R(i), imm — load an immediate value into a general register. Example: `LOADI R2, 10`
- LOADA A(i), addr — load a literal address into an address register. Example: `LOADA A0, 0x2002`
- LOADM R(i), (addr|A(j)) — load from memory into a register. Example: `LOADM R2, (A0)`
- STOREM (addr|A(j)), R(i) — store a register value into memory. Example: `STOREM (A0), R2`
- ADD R(i), R(j) — integer add: `ADD R1, R2`
- SUB R(i), R(j) — integer subtract: `SUB R1, R2`
- MLP R(i), R(j) — multiply (implementation detail in code). Example: `MLP R1, R2`
- DIV R(i), R(j) — divide (watch for divide-by-zero behavior). Example: `DIV R1, R2`
- AND/OR/XOR R(i), R(j) — bitwise logical ops.
- JMP addr — unconditional jump.
- JZ addr / JNZ addr — conditional jumps depending on the zero flag.
- CMP R(i), R(j) — compare two registers; sets flags used by conditional branches.
- HALT — stop execution.

See `include/isa.h` for the exact mnemonics, enum values, and comments.

How to run your own programs

1. Write an assembly file and save it into `asm-programs/` (copy an example and edit it).
2. The assembler in `src/` turns that file into machine words in RAM. By default `main` assembles `asm-programs/add.asm` — to test another program either:
   - Edit `src/main.c` to point `input_file` to your file, then rebuild; or
   - Run the binary from an IDE and change the path at runtime if you prefer to hack faster.

Testing and debugging tips

- Use `cpu_print` (available in the code) to inspect registers and flags after execution.
- The assembler and parser contain helpful error messages on invalid input.
- If you want a nicer CLI, consider adding argument parsing so `main` accepts a path.

Common next steps (ideas)

- Replace `main` hard-coded path with CLI args (argparse or simple `argc/argv`).
- Add more example programs (I/O, simple algorithms, stack, function calls).
- Implement more flags (carry, overflow) and richer debugging output.
- Add unit tests for the assembler and CPU execution.

Contributing

If you want to help: fork, make changes, and open a PR. Keep changes small and focused. If you add features, add or update examples in `asm-programs/`.

License

This project doesn't currently include a license file. If you want this to be MIT/BSD/whatever, add a `LICENSE` file and I'll update the README to match.

Acknowledgements / final note

This project is intentionally small. It's meant to be readable, educational, and easy to extend. If something here looks weird — that's intentional or because I'm experimenting. Feel free to open an issue or a PR.