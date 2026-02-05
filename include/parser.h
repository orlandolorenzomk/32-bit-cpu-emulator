//
// Created by dev on 2/2/26.
//

#ifndef INC_8BIT_CPU_EMULATOR_PARSER_H
#define INC_8BIT_CPU_EMULATOR_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "isa.h" /* for Label and MAX_LABELS */

/**
 * @brief Generic failure return value used by parser functions.
 */
#define FAILURE (-1)

/* String/line utilities */
/** Remove leading spaces/tabs from a line in-place. */
void remove_leading_whitespaces(char *line);
/** Remove trailing whitespace (spaces, tabs, newline, carriage return). */
void remove_trailing_whitespaces(char *line);
/** Strip assembler-style comments beginning with ';' from a line. */
void strip_comments(char *line);
/** Return true if a line is empty or contains only whitespace. */
bool is_empty_line(const char *line);

/* Opcode/operand parsing */
/** Translate mnemonic text into opcode integer (or FAILURE). */
int get_opcode(const char *mnemonic, const size_t opcode_table_size);
/** Parse general-purpose register token "R<n>" and return index or FAILURE. */
int parse_register(const char *reg);
/** Parse assembler directive (currently supports .org); returns numeric argument or FAILURE. */
int parse_directive(const char *line);
/** Parse a parenthesized memory operand and indicate whether it's a literal address via literal_addr pointer. */
int parse_address_parenthesis(const char *addr, bool *literal_addr);
/** Parse an address token either as a literal (0xNNNN) or as an address register (A<n>) depending on literal_addr flag. */
int parse_address(const char *addr, bool literal_addr);

/* Parse a parenthesized memory operand. If is_literal is non-NULL it will
 * be set to true when the inner token is a literal address (0x...), or false
 * when it's a register-indirect form (A<n>). Returns parsed value or FAILURE. */
int parse_address_register_from_parens(const char *addr, bool *is_literal);

/* Labels */
/** Return true if the given line ends with ':' indicating a label. */
bool is_label(const char *line);
/** Add a label with resolved address to the labels array; returns 0 on success. */
int add_label(const char *label_name, uint32_t addr, Label labels[], size_t *label_count);
/** Find a label's resolved address; returns address or FAILURE if not found. */
int find_label_addr(const char *label_name, Label labels[], size_t label_count);

#endif //INC_8BIT_CPU_EMULATOR_PARSER_H

