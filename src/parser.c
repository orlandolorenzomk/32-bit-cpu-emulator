#include <string.h>
#include <stdlib.h>
#include "cpu.h"
#include "isa.h"
#include "log.h"

#define ORG_DIRECTIVE ".org"
#define LABEL_ENDING ':'
#define FAILURE (-1)

/**
 * @brief Remove leading spaces and tabs from a string in-place.
 *
 * @param line NUL-terminated string to trim. If NULL the function returns immediately.
 * @return void
 * @note Modifies the input buffer by shifting characters left and NUL-terminating.
 */
void remove_leading_whitespaces(char *line)
{
	if (!line)
	{
		return;
	}

	const char *p = line;

	while (*p == ' ' || *p == '\t')
	{
		p++;
	}

	if (p != line)
	{
		char *dst = line;
		while (*p)
		{
			*dst++ = *p++;
		}
		*dst = '\0';
	}
}

/**
 * @brief Remove trailing whitespace and newline characters from a string in-place.
 *
 * @param line NUL-terminated string to trim. If NULL the function returns immediately.
 * @return void
 * @note Removes ' ', '\t', '\n', and '\r' from the end of the string.
 */
void remove_trailing_whitespaces(char *line)
{
	if (!line)
		return;

	char *p = line;
	while (*p)
		p++; // go to end

	while (p > line && (*(p - 1) == ' ' || *(p - 1) == '\t' || *(p - 1) == '\n' || *(p - 1) == '\r'))
	{
		p--;
		*p = '\0';
	}
}

/**
 * @brief Truncate the input string at the first ';' character (comment start).
 *
 * @param line NUL-terminated string to process. If NULL the function returns immediately.
 * @return void
 * @note Helpful for removing assembler comments that start with ';'.
 */
void strip_comments(char *line)
{
	if (!line)
		return;

	char *p = line;
	while (*p && *p != ';')
		p++;

	if (*p == ';')
	{
		*p = '\0';
	}
}

/**
 * @brief Determine whether a line is empty or contains only whitespace.
 *
 * @param line NUL-terminated input string. May be NULL.
 * @return true if the line is NULL or contains only whitespace characters,
 *         false otherwise.
 */
bool is_empty_line(const char *line)
{
	if (!line)
		return true;

	while (*line)
	{
		if (*line != ' ' && *line != '\t' && *line != '\n' && *line != '\r')
		{
			return false;
		}
		line++;
	}
	return true;
}

/**
 * @brief Lookup the numeric opcode for a textual mnemonic.
 *
 * @param mnemonic NUL-terminated instruction name (e.g. "LOADI").
 * @param opcode_table_size Number of entries in the opcode_table array.
 * @return opcode as non-negative integer on success, -1 if the mnemonic is not found.
 */
int get_opcode(const char *mnemonic, const size_t opcode_table_size)
{
	for (size_t i = 0; i < opcode_table_size; i++)
	{
		if (strcmp(mnemonic, opcode_table[i].mnemonic) == 0)
		{
			return (int)opcode_table[i].opcode;
		}
	}

	return FAILURE;
}

/**
 * @brief Parse a general-purpose register token of the form 'R<n>'.
 *
 * @param reg Token starting with 'R' followed by a single digit.
 * @return Register index (>=0) on success, -1 on error.
 * @note Logs an error message on invalid input.
 */
int parse_register(const char *reg)
{
	if (!reg) {
		log_write(LOG_ERROR, "Register pointer is NULL");
		return FAILURE;
	}

	if (reg[0] != 'R') {
		log_write(LOG_ERROR, "Registers must start with 'R'");
		return FAILURE;
	}

	char *end = NULL;
	long index = strtol(reg + 1, &end, 10);

	if (*end != '\0') {
		log_write(LOG_ERROR, "Invalid register format: %s", reg);
		return FAILURE;
	}

	if (index < 0 || index >= MAX_REGISTERS) {
		log_write(
			LOG_ERROR,
			"Available registers are R0 - R%u",
			MAX_REGISTERS - 1
		);
		return FAILURE;
	}

	return (int)index;
}

/**
 * @brief Parse an assembler directive line (currently supports .org).
 *
 * @param line NUL-terminated directive line (e.g. ".org 0x2001").
 * @return Integer argument parsed from the directive on success, or -1 if
 *         the line does not contain the recognized directive.
 */
int parse_directive(const char *line)
{
	const size_t size = strlen(ORG_DIRECTIVE);
	if (strncmp(line, ORG_DIRECTIVE, size) == 0)
	{
		const char *p = line + size;
		while (*p == ' ' || *p == '\t')
			p++;
		return (int)strtol(p, (char **)nullptr, 0);
	}

	return FAILURE;
}

/**
 * @brief Parse either a numeric literal address (0xNNNN) or an address register (A<n>).
 *
 * @param addr Token to parse (NUL-terminated).
 * @param literal_addr If true expect a literal address of form 0xNNNN; if false expect 'A<n>'.
 * @return Numeric address or register index on success, -1 on error.
 * @note Logs descriptive errors when parsing fails.
 */
int parse_address_parenthesis(const char *addr, bool *literal_addr)
{
	if (addr == NULL)
	{
		log_write(LOG_ERROR, "Error: address pointer is NULL");
		return FAILURE;
	}

	bool is_valid_address = false;

	if (addr[0] == '(' && addr[1] == '0' && addr[2] == 'x') {
		is_valid_address = true;
		*literal_addr = true;
	}

	if (addr[0] == '(' && addr[1] == 'A') {
		is_valid_address = true;
		*literal_addr = false;
	}

	if (!is_valid_address) {
		log_write(LOG_ERROR, "Address is not valid");
		return FAILURE;
	}

	if (*literal_addr) {
		char *end;
		long addr_val = strtol(addr + 1, &end, 0);  // skip '('

		if (*end != ')') {
			log_write(LOG_ERROR, "Invalid literal address format: %s", addr);
			return FAILURE;
		}

		if (addr_val < 0 || addr_val > 0xFFFF) {
			log_write(LOG_ERROR, "Address out of range: %s", addr);
			return FAILURE;
		}

		return (int)addr_val;
	} else {
		int index = addr[2] - '0';
		if (index >= MAX_ADDRESS_REGISTERS) {
			log_write(LOG_ERROR, "Available addresses are A[0] - A[%u]", MAX_ADDRESS_REGISTERS - 1);
			return FAILURE;
		}

		return index;
	}
}

/**
 * @brief Parse either a numeric literal address (0xNNNN) or an address register (A<n>).
 *
 * @param addr Token to parse (NUL-terminated).
 * @param literal_addr If true expect a literal address of form 0xNNNN; if false expect 'A<n>'.
 * @return Numeric address or register index on success, -1 on error.
 * @note Logs descriptive errors when parsing fails.
 */
int parse_address(const char *addr, bool literal_addr)
{
	if (addr == NULL)
	{
		log_write(LOG_ERROR, "Error: address pointer is NULL");
		return FAILURE;
	}

	if (literal_addr)
	{
		if (addr[0] != '0' || addr[1] != 'x')
		{
			log_write(LOG_ERROR, "Address must have format 0x0000");
			return FAILURE;
		}

		long addr_val = strtol(addr, NULL, 0); // base 0 auto-detects 0x prefix
		if (addr_val < 0 || addr_val > 0xFFFF)
		{
			log_write(LOG_ERROR, "Address out of range: %s", addr);
			return FAILURE;
		}
		return (int)addr_val;
	}
	else
	{
		if (addr[0] != 'A')
		{
			log_write(LOG_ERROR, "Address must start with 'A'");
			return FAILURE;
		}

		int index = addr[1] - '0';
		if (index >= MAX_ADDRESS_REGISTERS)
		{
			log_write(LOG_ERROR, "Available addresses are A[0] - A[%u]", MAX_ADDRESS_REGISTERS - 1);
			return FAILURE;
		}

		return index;
	}
}

/**
 * @brief Return true if the given line ends with the label terminator ':' .
 *
 * @param line NUL-terminated input string to test.
 * @return true if the line ends with ':', false otherwise.
 */
bool is_label(const char *line)
{
	const size_t len = strlen(line);
	return (len > 0 && line[len - 1] == LABEL_ENDING);
}

/**
 * @brief Add a label name and resolved address to the labels table.
 *
 * @param label_name NUL-terminated label text.
 * @param addr Address to associate with the label.
 * @param labels Array of Label entries where the label will be stored.
 * @param label_count Pointer to the current label count; incremented on success.
 * @return 0 on success, -1 on error (e.g. table full).
 */
int add_label(const char *label_name, uint32_t addr, Label labels[], size_t *label_count)
{
	if (*label_count >= MAX_LABELS)
	{
		log_write(LOG_ERROR, "Maximum number of labels reached");
		return FAILURE;
	}
	strncpy(labels[*label_count].name, label_name, sizeof(labels[*label_count].name) - 1);
	labels[*label_count].name[sizeof(labels[*label_count].name) - 1] = '\0';
	labels[*label_count].address = addr;
	(*label_count)++;
	return 0;
}

/**
 * @brief Find a label by name and return its resolved address.
 *
 * @param label_name NUL-terminated label to search for.
 * @param labels Array of Label entries.
 * @param label_count Number of valid entries in labels[].
 * @return Resolved address (>=0) on success, -1 if the label is not found.
 */
int find_label_addr(const char *label_name, Label labels[], size_t label_count)
{
	for (size_t i = 0; i < label_count; i++)
	{
		if (strcmp(labels[i].name, label_name) == 0)
			return (int)labels[i].address;
	}
	log_write(LOG_ERROR, "Label not found: %s", label_name);
	return FAILURE;
}
