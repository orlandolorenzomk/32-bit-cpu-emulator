//
// Created by dev on 2/1/26.
//

#ifndef INC_8BIT_CPU_EMULATOR_RAM_H
#define INC_8BIT_CPU_EMULATOR_RAM_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/**
 * @brief Number of addressable 32-bit cells in RAM.
 *
 * RAM provides a flat indexable array of RAM_SIZE 32-bit words (0..RAM_SIZE-1).
 */
#define RAM_SIZE 65536

/**
 * @brief RAM instance holding the memory cells and synchronization primitive.
 *
 * The Ram structure contains the array of memory cells and a pthread
 * read-write lock used to synchronize concurrent accesses. Callers should
 * use the provided API functions (ram_init, ram_store, ram_load) rather
 * than manipulating fields directly.
 */
typedef struct {
    uint32_t cells[RAM_SIZE];
    pthread_rwlock_t lock;
} RAM;

/**
 * @brief Check whether a RAM address is within valid bounds.
 *
 * This helper verifies that the provided address is less than RAM_SIZE.
 * It also validates the `ram` pointer is non-NULL. The function is
 * thread-safe because it does not access mutable global state and does not
 * acquire locks; callers should still use the RAM object's locks when
 * performing actual memory operations.
 *
 * @param ram Pointer to the RAM instance (must not be NULL).
 * @param address The address to validate.
 * @return true if the address is valid (ram != NULL and address < RAM_SIZE), false otherwise.
 *
 */
bool is_address_valid (const RAM *ram, const uint32_t address);
/**
 * @brief Initialize a Ram instance.
 *
 * Prepares the RAM instance for use by zeroing memory and initializing the
 * internal read-write lock. The provided pointer must refer to valid
 * memory and be non-NULL.
 *
 * @param ram Pointer to a Ram structure to initialize (must be non-NULL).
 */
void ram_init(RAM *ram);

/**
 * @brief Store a 32-bit value at the specified RAM address.
 *
 * This function validates the address and uses an exclusive write lock to
 * perform the store. It returns false if the pointer is NULL or the address
 * is out of bounds.
 *
 * @param ram Pointer to an initialized Ram instance.
 * @param address Address where the value will be stored (0 <= address < RAM_SIZE).
 * @param value 32-bit value to store.
 * @return true on success, false on error (NULL pointer or invalid address).
 */
bool ram_store(RAM *ram, uint32_t address, uint32_t value);

/**
 * @brief Load a 32-bit value from the specified RAM address.
 *
 * This function validates the address and uses a shared read lock to allow
 * concurrent readers. The read value is written to the location pointed to
 * by output.
 *
 * @param ram Pointer to an initialized Ram instance.
 * @param address Address to read from (0 <= address < RAM_SIZE).
 * @param output Pointer to an uint32_t where the read value will be stored
 * (must be non-NULL).
 * @return true on success and *output is set, false on error (NULL pointer or invalid address).
 */
bool ram_load(RAM *ram, uint32_t address, uint32_t *output);

/**
 * @brief Clear (free) a contiguous range of RAM cells.
 *
 * This function zeros the RAM cells in the range [start, end] (inclusive)
 * using an exclusive write lock to ensure thread-safety. It is a convenience
 * helper that can be used by the assembler, loader, or tests to mark a
 * region of RAM as unused by zeroing its contents.
 *
 * @param ram Pointer to an initialized RAM instance (must be non-NULL).
 * @param start Start address of the range to clear (0 <= start < RAM_SIZE).
 * @param end End address of the range to clear (start <= end < RAM_SIZE).
 * @return true on success (range cleared), false on error (NULL pointer,
 *         invalid range, or lock/init failure).
 *
 * @note The function acquires the RAM instance's write lock while clearing.
 *       Callers should not attempt to access the cleared region concurrently
 *       until the function returns.
 */
bool ram_free(RAM *ram, uint32_t start, uint32_t end);

#endif //INC_8BIT_CPU_EMULATOR_RAM_H

