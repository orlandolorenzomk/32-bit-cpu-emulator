//
// Created by dev on 2/2/26.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "assembler.h"

#include <stdarg.h>
#include <errno.h>

#include "log.h"
#include "parser.h"
#include "../include/validation.h"

/**
 * @brief Initialize an AssemblyRange structure to a default successful state.
 *
 * @param range Pointer to AssemblyRange to initialize (must not be NULL).
 * @return void
 */
static void initialize_assembly(AssemblyRange *range) {
    range->start_address = 0;
    range->end_address = 0;
    range->error = false;
}

/**
 * @brief Produce an AssemblyRange that signals an assembly error and close a file.
 *
 * This helper is used when assembly fails and the assembler needs to
 * close any open FILE* and return an error sentinel to the caller.
 *
 * @param file FILE pointer to close; may be NULL.
 * @return AssemblyRange with `error == true`.
 * @note The function closes `file` if non-NULL and returns a range with
 *       start/end addresses set to zero and `error` set to true.
 */
static AssemblyRange assemble_error(FILE *file) {
    if (file) {
        fclose(file);
    }
    AssemblyRange range;
    range.start_address = 0;
    range.end_address = 0;
    range.error = true;
    return range;
}

/**
 * @brief Emit a 32-bit word into RAM at the CPU's current PC and increment PC.
 *
 * This helper centralizes writing a value into RAM and advancing the
 * program counter. It expects `ram` and `cpu` to be valid and that CPU's
 * `pc` points to a writable location in `ram->cells`.
 *
 * @param ram Pointer to RAM to write into.
 * @param cpu Pointer to CPU whose PC will be updated.
 * @param value 32-bit value to write into the current PC slot.
 */
static void emit(RAM *ram, CPU *cpu, uint32_t value) {
    ram->cells[cpu->pc++] = value;
}

/**
 * @brief Validate that two operands were provided for a two-operand instruction.
 *
 * Logs an error if either operand is NULL.
 *
 * @param line_num Source line number (for logging).
 * @param op1 First operand string (may be NULL).
 * @param op2 Second operand string (may be NULL).
 * @return true if both operands are non-NULL; false otherwise.
 */
static bool require_two_operands(size_t line_num, char *op1, char *op2) {
    if (!op1 || !op2) {
        log_write(LOG_ERROR, "[Line %zu] Instruction requires 2 operands", line_num);
        return false;
    }
    return true;
}

/**
 * @brief Validate that a single operand was provided for a one-operand instruction.
 *
 * Logs an error if the operand is NULL.
 *
 * @param line_num Source line number (for logging).
 * @param op1 Operand string (may be NULL).
 * @return true if the operand is non-NULL; false otherwise.
 */
static bool require_one_operand(size_t line_num, char *op1) {
    if (!op1) {
        log_write(LOG_ERROR, "[Line %zu] Instruction requires 1 operand", line_num);
        return false;
    }
    return true;
}

/**
 * @brief Variadic helper that checks a list of integer return values for FAILURE.
 *
 * This function inspects `count` integers from the variadic list (each
 * expected to be an int return code such as a parsed register/index). If any
 * are equal to FAILURE the function logs `message` with the supplied line
 * number and returns false.
 *
 * @param line_num Source line number for logging.
 * @param message Error message to log when a FAILURE is detected.
 * @param count Number of int values passed after this argument.
 * @param ... int values to check (use FAILURE sentinel to indicate an error).
 * @return true if all provided ints are != FAILURE; false if any == FAILURE.
 */
static bool require_all_not_failure(
    size_t line_num,
    const char *message,
    size_t count,
    ...
) {
    va_list args;
    va_start(args, count);

    for (size_t i = 0; i < count; i++) {
        int value = va_arg(args, int);
        if (value == FAILURE) {
            log_write(LOG_ERROR, "[Line %zu] %s", line_num, message);
            va_end(args);
            return false;
        }
    }

    va_end(args);
    return true;
}

/**
 * @brief Handle generic arithmetic instruction (e.g., ADD, SUB, AND...).
 *
 * Parses two register operands, validates them and emits the opcode followed
 * by destination and source register indices into RAM.
 *
 * @param ram Destination RAM.
 * @param cpu CPU context (used for emitting at cpu->pc).
 * @param line_num Source line number for logging.
 * @param opcode The numeric ISA opcode to emit.
 * @param op1 Destination operand text (e.g., "R0").
 * @param op2 Source operand text (e.g., "R1").
 * @return true on success (operands parsed and emitted), false on error.
 */
static bool handle_arithmetic_instruction(
    RAM *ram, CPU *cpu,
    size_t line_num,
    uint32_t opcode,
    char *op1, char *op2
) {
    if (!require_two_operands(line_num, op1, op2))
        return false;

    int dst = parse_register(op1);
    if (!require_all_not_failure(line_num, "Register operand error", 1, dst))
        return false;
    if (!is_reg_index_valid_asm(dst)) {
        log_write(LOG_ERROR, "[Line %zu] Invalid destination register: %s", line_num, op1);
        return false;
    }

    int src_reg = parse_register(op2);
    if (src_reg != FAILURE) {
        /* Register-source encoding: opcode, dst, mode=OPERAND_REGISTER, operand=src_reg */
        emit(ram, cpu, opcode);
        emit(ram, cpu, (uint32_t)dst);
        emit(ram, cpu, OPERAND_REGISTER);
        emit(ram, cpu, (uint32_t)src_reg);
        return true;
    }

    /* Otherwise treat op2 as an immediate numeric value. Accept optional '#' prefix. */
    const char *p = op2;
    if (!p) {
        log_write(LOG_ERROR, "[Line %zu] Missing source operand", line_num);
        return false;
    }
    if (p[0] == '#') p++;

    char *endptr = NULL;
    errno = 0;
    long sval = strtol(p, &endptr, 0);
    if (endptr == p || *endptr != '\0') {
        log_write(LOG_ERROR, "[Line %zu] Invalid numeric operand: %s", line_num, op2);
        return false;
    }
    if (errno == ERANGE || sval < 0) {
        /* allow negative values by casting to uint32_t, but flag very large negative as error */
        if (errno == ERANGE) {
            log_write(LOG_ERROR, "[Line %zu] Numeric operand out of range: %s", line_num, op2);
            return false;
        }
    }

    emit(ram, cpu, opcode);
    emit(ram, cpu, (uint32_t)dst);
    emit(ram, cpu, OPERAND_NUMERIC);
    emit(ram, cpu, (uint32_t)sval);
    return true;
}

/**
 * @brief Handle the LOADI instruction (register, immediate).
 *
 * Syntax: LOADI Rn, #imm   or LOADI Rn, imm
 * Emits:  ISA_LOADI, reg_index, immediate_value
 *
 * @param ram Destination RAM.
 * @param cpu CPU context for emitting.
 * @param line_num Source line (for logging).
 * @param op1 Destination register token.
 * @param op2 Immediate token (may start with '#').
 * @return true on success, false on parse error.
 */
static bool handle_loadi_instruction(
    RAM *ram, CPU *cpu,
    size_t line_num,
    char *op1, char *op2
) {
    if (!require_two_operands(line_num, op1, op2))
        return false;

    int register_index = parse_register(op1);
    if (!require_all_not_failure(
        line_num,
        "Register operand error",
        1,
        register_index
    )) {
        return false;
    }

    if (op2[0] == '#') op2++;

    uint32_t imm = (uint32_t) strtol(op2, NULL, 0);

    emit(ram, cpu, ISA_LOADI);
    emit(ram, cpu, (uint32_t) register_index);
    emit(ram, cpu, imm);

    return true;
}

/**
 * @brief Handle the LOADA instruction (address register, address literal).
 *
 * Syntax: LOADA Ai, addr
 * Emits: ISA_LOADA, addr_register_index, address_literal
 *
 * The assembler's `parse_address` returns a register index for the address
 * register operand (op1) and a literal address for op2. Both values are
 * validated before being emitted.
 */
static bool handle_loada_instruction(
    RAM *ram, CPU *cpu,
    size_t line_num,
    char *op1, char *op2
) {
    if (!require_two_operands(line_num, op1, op2))
        return false;

    int addr_reg = parse_address(op1, false);
    int addr_lit = parse_address(op2, true);

    if (!require_all_not_failure(
        line_num,
        "Address operand error",
        2,
        addr_reg,
        addr_lit
    )) {
        return false;
    }

    if (!is_addr_index_valid_asm(addr_reg)) {
        log_write(LOG_ERROR, "[Line %zu] Invalid address register: %s", line_num, op1);
        return false;
    }

    if (!is_addr_literal_valid((uint32_t)addr_lit)) {
        log_write(LOG_ERROR, "[Line %zu] Invalid literal address: %s", line_num, op2);
        return false;
    }

    emit(ram, cpu, ISA_LOADA);
    emit(ram, cpu, (uint32_t)addr_reg);
    emit(ram, cpu, (uint32_t)addr_lit);

    return true;
}

/**
 * @brief Handle the LOADM instruction (load from memory into register).
 *
 * Syntax: LOADM Rn, (Ai)    or   LOADM Rn, (0xADDR)
 * Emits: ISA_LOADM, reg_index, mode_flag, operand
 *  - mode_flag = ADDR_LITERAL or ADDR_REGISTER
 *  - operand = literal address or address-register index
 *
 * The helper `parse_address_parenthesis` is expected to accept both forms
 * and set `is_literal` accordingly.
 */
static bool handle_loadm_instruction(
    RAM *ram, CPU *cpu,
    size_t line_num,
    char *op1, char *op2
) {
    if (!require_two_operands(line_num, op1, op2))
        return false;

    int register_index = parse_register(op1);

    bool is_literal = false;
    int addr = parse_address_parenthesis(op2, &is_literal);

    if (!require_all_not_failure(
            line_num,
            "Operand error",
            2,
            register_index,
            addr
        ))
        return false;

    if (!is_reg_index_valid_asm(register_index)) {
        log_write(LOG_ERROR, "[Line %zu] Invalid register: %s", line_num, op1);
        return false;
    }

    if (is_literal) {
        if (!is_addr_literal_valid((uint32_t)addr)) {
            log_write(LOG_ERROR, "[Line %zu] Invalid literal address: %s", line_num, op2);
            return false;
        }
    } else {
        if (!is_addr_index_valid_asm(addr)) {
            log_write(LOG_ERROR, "[Line %zu] Invalid address register: %s", line_num, op2);
            return false;
        }
    }

    emit(ram, cpu, ISA_LOADM);
    emit(ram, cpu, (uint32_t)register_index);
    emit(ram, cpu, is_literal ? ADDR_LITERAL : ADDR_REGISTER);
    emit(ram, cpu, (uint32_t)addr);

    return true;
}

/**
 * @brief Handle the STOREM instruction (store register into memory).
 *
 * Syntax: STOREM (Ai) , Rn    or   STOREM (0xADDR), Rn
 * Emits: ISA_STOREM, addr_operand, mode_flag, register_index
 *
 * Note: The assembler encodes the address operand first, then a mode flag
 * and finally the register index to store.
 */
static bool handle_storem_instruction(
    RAM *ram, CPU *cpu,
    size_t line_num,
    char *op1, char *op2
) {
    if (!require_two_operands(line_num, op1, op2)) {
        return false;
    }

    int register_index = parse_register(op2);

    bool is_literal = false;
    int addr = parse_address_parenthesis(op1, &is_literal);

    if (!require_all_not_failure(
        line_num,
        "Operand error",
        2,
        register_index,
        addr
    )) {
        return false;
    }

    if (!is_reg_index_valid_asm(register_index)) {
        log_write(LOG_ERROR, "[Line %zu] Invalid register: %s", line_num, op2);
        return false;
    }

    if (is_literal) {
        if (!is_addr_literal_valid((uint32_t)addr)) {
            log_write(LOG_ERROR, "[Line %zu] Invalid literal address: %s", line_num, op1);
            return false;
        }
    } else {
        if (!is_addr_index_valid_asm(addr)) {
            log_write(LOG_ERROR, "[Line %zu] Invalid address register: %s", line_num, op1);
            return false;
        }
    }

    emit(ram, cpu, ISA_STOREM);
    emit(ram, cpu, (uint32_t)addr);
    emit(ram, cpu, is_literal ? ADDR_LITERAL : ADDR_REGISTER);
    emit(ram, cpu, (uint32_t) register_index);

    return true;
}

/**
 * @brief Handle the CMP instruction (compare two registers and set flags).
 *
 * Syntax: CMP Rn, Rm
 * Emits: ISA_CMP, reg_n_index, reg_m_index
 *
 * The assembler emits the CMP opcode followed by the two register indices.
 * The runtime `CMP` handler will set CPU flags (zero/negative) and does not
 * write back to registers. This assembler helper validates the two register
 * operands before emission.
 */
static bool handle_cmp_instruction(
    RAM *ram, CPU *cpu,
    size_t line_num,
    char *op1, char *op2
) {
    if (!require_two_operands(line_num, op1, op2)) {
        return false;
    }

    int dst_index = parse_register(op1);
    int src_index = parse_register(op2);

    if (!require_all_not_failure(
        line_num,
        "Register error",
        2,
        dst_index,
        src_index)) {
        return false;
    }

    if (!is_reg_index_valid_asm(dst_index)) {
        log_write(LOG_ERROR, "[Line %zu] Invalid register: %s", line_num, op1);
        return false;
    }
    if (!is_reg_index_valid_asm(src_index)) {
        log_write(LOG_ERROR, "[Line %zu] Invalid register: %s", line_num, op2);
        return false;
    }

    emit(ram, cpu, ISA_CMP);
    emit(ram, cpu, (uint32_t)dst_index);
    emit(ram, cpu, (uint32_t)src_index);

    return true;
}

/**
 * @brief Assemble a jump-like instruction (JMP, JZ, JNZ).
 *
 * This helper resolves the operand `op1` which may be a numeric literal
 * (0xNNNN) or a label name. On success it emits the opcode and resolved
 * address into RAM (using `emit`) and returns true.
 */
static bool handle_jmp_instructions(
    RAM *ram, CPU *cpu,
    size_t line_num,
    int opcode,
    char *op1,
    Label *labels,
    size_t label_count
) {
    if (!require_one_operand(line_num, op1))
        return false;

    int addr = FAILURE;
    /* If token looks like a literal address parse it, otherwise treat as label */
    if (op1[0] == '0' && (op1[1] == 'x' || op1[1] == 'X')) {
        addr = parse_address(op1, true);
    } else {
        addr = find_label_addr(op1, labels, label_count);
    }

    if (addr == FAILURE) {
        log_write(LOG_ERROR, "[Line %zu] Address literal error", line_num);
        return false;
    }

    emit(ram, cpu, opcode);
    emit(ram, cpu, (uint32_t)addr);
    return true;
}


/**
 * @brief Assemble the source file into RAM.
 *
 * The assembler reads the source file line-by-line, strips whitespace and
 * comments, handles directives (for example .org via parse_directive),
 * records labels, and emits opcodes and operands into `ram->cells` using
 * the CPU's `pc` as the write cursor.
 *
 * This function performs two-pass style responsibilities in a single pass
 * for simplicity: it stores labels as they are encountered and emits code
 * immediately. The caller must provide an initialized `RAM` and `CPU`.
 *
 * @param ram Pointer to initialized RAM where code will be written.
 * @param cpu CPU context used for program counter and label insertion.
 * @param file_path Path to assembly source to open.
 * @return AssemblyRange indicating the start and end addresses of the
 *         emitted code; on error the returned range has `error == true`.
 */
AssemblyRange assemble(RAM *ram, CPU *cpu, const char *file_path) {
    if (!ram || !cpu || !file_path) {
        log_write(LOG_ERROR, "Assemble failed: NULL argument(s) provided");
        return assemble_error(NULL);
    }

    FILE *file = fopen(file_path, "r");
    if (!file) {
        log_write(LOG_ERROR, "Unable to open assembly file: %s", file_path);
        return assemble_error(NULL);
    }

    AssemblyRange range;
    initialize_assembly(&range);

    Label labels[MAX_LABELS];
    size_t label_count = 0;

    size_t opcode_table_size = sizeof(opcode_table) / sizeof(opcode_table[0]);

    /* Read file lines into memory for two-pass processing */
    char **lines = NULL;
    size_t lines_count = 0;
    char buf[1024];
    while (fgets(buf, sizeof(buf), file)) {
        lines = realloc(lines, sizeof(*lines) * (lines_count + 1));
        lines[lines_count] = strdup(buf);
        lines_count++;
    }

    if (lines == NULL) {
        log_write(LOG_ERROR, "Lines is NULL");
        return assemble_error(file);
    }

    /* First pass: discover labels and compute address offsets */
    uint32_t pc_cursor = 0;
    bool start_set = false;

    for (size_t i = 0; i < lines_count; ++i) {
        char *line = lines[i];
        remove_leading_whitespaces(line);
        remove_trailing_whitespaces(line);
        strip_comments(line);

        if (is_empty_line(line))
            continue;

        if (line[0] == '.') {
            log_write(LOG_DEBUG, "First pass line %zu (directive candidate): '%s'", i + 1, line);
        }

        /* Handle directives like .org before any label/mnemonic processing */
        int directive_val = parse_directive(line);
        /* Fallback: if parse_directive failed but the line starts with '.' try manual parse */
        if (directive_val == FAILURE && line[0] == '.') {
            char tmp_dir[1024];
            strncpy(tmp_dir, line, sizeof(tmp_dir)-1);
            tmp_dir[sizeof(tmp_dir)-1] = '\0';
            char *tok = strtok(tmp_dir, " \t");
            if (tok && strcmp(tok, ".org") == 0) {
                char *arg = strtok(NULL, " \t");
                if (arg) {
                    long v = strtol(arg, NULL, 0);
                    directive_val = (int)v;
                }
            }
        }
         if (directive_val != FAILURE) {
             pc_cursor = (uint32_t)directive_val;
             if (!start_set) {
                 range.start_address = pc_cursor;
                 start_set = true;
             }
             continue;
         }

        /* Tokenize the line to detect a label (first token ending with ':') or a mnemonic. */
        char tmp[1024];
        strncpy(tmp, line, sizeof(tmp) - 1);
        tmp[sizeof(tmp)-1] = '\0';
        char *first_tok = strtok(tmp, " ,\t");
        if (!first_tok)
            continue;

        size_t ftlen = strlen(first_tok);
        char *mn = NULL;
        /* Prepare placeholders for first-pass operand tokens (may be used to compute size) */
        char *fp_op1 = NULL;
        char *fp_op2 = NULL;
         if (ftlen > 0 && first_tok[ftlen - 1] == ':') {
            /* label detected */
            char label_name[256];
            size_t copy_len = (ftlen - 1 < sizeof(label_name)-1) ? ftlen - 1 : sizeof(label_name)-1;
            strncpy(label_name, first_tok, copy_len);
            label_name[copy_len] = '\0';
            if (add_label(label_name, pc_cursor, labels, &label_count) == FAILURE) {
                log_write(LOG_ERROR, "Failed to add label: %s", label_name);
                for (size_t j = 0; j < lines_count; ++j) free(lines[j]);
                free(lines);
                fclose(file);
                return assemble_error(NULL);
            }
            log_write(LOG_DEBUG, "Found label '%s' at word address %u (first pass)", label_name, pc_cursor);

            /* check if instruction follows on the same line */
            char *rest = strtok(NULL, " ,\t");
            if (!rest)
                continue; /* label-only line */
            mn = rest;
            /* capture possible operands for first-pass sizing */
            fp_op1 = strtok(NULL, " ,\t");
            fp_op2 = strtok(NULL, " ,\t");
         } else {
             mn = first_tok;
            /* capture operands when label not present */
            fp_op1 = strtok(NULL, " ,\t");
            fp_op2 = strtok(NULL, " ,\t");
         }


         int opcode = get_opcode(mn, opcode_table_size);
         if (opcode == FAILURE) {
            log_write(LOG_ERROR, "[Line %zu] Invalid mnemonic: %s", i + 1, mn);
            for (size_t j = 0; j < lines_count; ++j) free(lines[j]);
            free(lines);
            fclose(file);
            return assemble_error(NULL);
         }

        /* instruction sizes (words)
           Note: some arithmetic-like opcodes can take either a register or a numeric
           source operand. If the source is numeric we emit an extra mode + literal
           word, so we must account for that here. We use the captured fp_op2 token
           to decide sizing. */
        switch (opcode) {
             case ISA_LOADI: pc_cursor += 3; break;
             case ISA_LOADA: pc_cursor += 3; break;
             case ISA_LOADM: pc_cursor += 4; break;
             case ISA_STOREM: pc_cursor += 4; break;
            case ISA_ADD: case ISA_SUB: case ISA_MLP: case ISA_DIV:
            case ISA_AND: case ISA_OR: case ISA_XOR:
                /* Emit: opcode, dst, mode_flag, operand -> always 4 words */
                pc_cursor += 4;
                break;
             case ISA_JMP: case ISA_JZ: case ISA_JNZ:
                 pc_cursor += 2; break;
             case ISA_HALT:
                 pc_cursor += 1; break;
             default:
                 /* Conservative: assume 1 word to avoid skipping labels */
                 pc_cursor += 1; break;
         }
     }

    /* If start wasn't set by a directive, default to 0 */
    if (!start_set) {
        range.start_address = 0;
    }

    /* Second pass: emit instructions, resolving labels */
    cpu->pc = range.start_address;

    for (size_t i = 0; i < lines_count; ++i) {
        char *line = lines[i];
        remove_leading_whitespaces(line);
        remove_trailing_whitespaces(line);
        strip_comments(line);

        if (is_empty_line(line))
            continue;

        /* Debug: log lines starting with '.' to inspect directives */
        if (line[0] == '.') {
            log_write(LOG_DEBUG, "Second pass line %zu (directive candidate): '%s'", i + 1, line);
        }

        /* If the line contains a label (e.g. 'label:'), ignore the label and parse the remainder. */
        char *text = line;
        char *colon = strchr(line, ':');
        if (colon) {
            text = colon + 1;
            remove_leading_whitespaces(text);
            if (is_empty_line(text))
                continue; /* label-only line */
        }

        int directive_val = parse_directive(text);
        /* Fallback: if parse_directive failed but the line starts with '.' try manual parse */
        if (directive_val == FAILURE && text[0] == '.') {
            char tmp_dir[1024];
            strncpy(tmp_dir, text, sizeof(tmp_dir)-1);
            tmp_dir[sizeof(tmp_dir)-1] = '\0';
            char *tok = strtok(tmp_dir, " \t");
            if (tok && strcmp(tok, ".org") == 0) {
                char *arg = strtok(NULL, " \t");
                if (arg) {
                    long v = strtol(arg, NULL, 0);
                    directive_val = (int)v;
                }
            }
        }
        if (directive_val != FAILURE) {
            cpu->pc = (uint32_t)directive_val;
            continue;
        }

        /* Tokenize the instruction from 'text' (work on a temp buffer so lines[] isn't modified) */
        char work[1024];
        strncpy(work, text, sizeof(work)-1);
        work[sizeof(work)-1] = '\0';
        char *mnemonic = strtok(work, " ,\t");
        char *op1 = strtok(NULL, " ,\t");
        char *op2 = strtok(NULL, " ,\t");

        if (!mnemonic)
            continue;

        int opcode = get_opcode(mnemonic, opcode_table_size);
        if (opcode == FAILURE) {
            log_write(LOG_ERROR, "[Line %zu] Invalid mnemonic: %s", i + 1, mnemonic);
            for (size_t j = 0; j < lines_count; ++j) free(lines[j]);
            free(lines);
            fclose(file);
            return assemble_error(NULL);
        }

        switch (opcode) {
            case ISA_LOADI:
                if (!handle_loadi_instruction(ram, cpu, i + 1, op1, op2)) {
                    for (size_t j = 0; j < lines_count; ++j) free(lines[j]); free(lines); fclose(file);
                    return assemble_error(NULL);
                }
                break;

            case ISA_LOADA:
                if (!handle_loada_instruction(ram, cpu, i + 1, op1, op2)) {
                    for (size_t j = 0; j < lines_count; ++j) free(lines[j]); free(lines); fclose(file);
                    return assemble_error(NULL);
                }
                break;

            case ISA_LOADM:
                if (!handle_loadm_instruction(ram, cpu, i + 1, op1, op2)) {
                    for (size_t j = 0; j < lines_count; ++j) free(lines[j]); free(lines); fclose(file);
                    return assemble_error(NULL);
                }
                break;

            case ISA_STOREM:
                if (!handle_storem_instruction(ram, cpu, i + 1, op1, op2)) {
                    for (size_t j = 0; j < lines_count; ++j) free(lines[j]); free(lines); fclose(file);
                    return assemble_error(NULL);
                }
                break;

            case ISA_ADD:
            case ISA_SUB:
            case ISA_MLP:
            case ISA_DIV:
            case ISA_AND:
            case ISA_OR:
            case ISA_XOR:
                if (!handle_arithmetic_instruction(ram, cpu, i + 1, opcode, op1, op2)) {
                    for (size_t j = 0; j < lines_count; ++j) free(lines[j]); free(lines); fclose(file);
                    return assemble_error(NULL);
                }
                break;

            case ISA_CMP:
                if (!handle_cmp_instruction(ram, cpu, i + 1, op1, op2)) {
                    for (size_t j = 0; j < lines_count; ++j) free(lines[j]); free(lines); fclose(file);
                    return assemble_error(NULL);
                }
                break;

            case ISA_JMP:
            case ISA_JZ:
            case ISA_JNZ: {
                if (!handle_jmp_instructions(ram, cpu, i + 1, opcode, op1, labels, label_count)) {
                    for (size_t j = 0; j < lines_count; ++j) free(lines[j]); free(lines); fclose(file);
                    return assemble_error(NULL);
                }
                break;
            }

            case ISA_HALT:
                emit(ram, cpu, ISA_HALT);
                break;

            default:
                log_write(LOG_ERROR, "[Line %zu] Unhandled opcode %s", i + 1, mnemonic);
                for (size_t j = 0; j < lines_count; ++j) free(lines[j]); free(lines); fclose(file);
                return assemble_error(NULL);
        }
    }

    range.end_address = cpu->pc;

    for (size_t j = 0; j < lines_count; ++j) free(lines[j]);
    free(lines);
    fclose(file);
    return range;
}
