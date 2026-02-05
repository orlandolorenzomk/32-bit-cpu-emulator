//
// Created by dev on 2/1/26.
//

#include "ram.h"
#include "log.h"

#include <string.h>

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
bool is_address_valid (const RAM *ram, const uint32_t address) {
    if (!ram) {
        return false;
    }
    return address < (uint32_t) RAM_SIZE;
}

/**
 * @brief Initialize a Ram instance.
 *
 * Initializes the memory cells to zero and sets up the internal
 * pthread read-write lock used for concurrent access.
 *
 * @param ram Pointer to a Ram structure to initialize.
 *
 * @note After this call the RAM instance is ready for concurrent use with
 * ram_store and ram_load which use the internal read-write lock.
 *
 * @sideeffects Initializes ram->lock and zeroes ram->cells and logs an
 * informational message via log_write.
 */
void ram_init(RAM *ram) {
    if (!ram) {
        log_write(LOG_ERROR, "RAM initialization failed: RAM pointer is NULL");
        return;
    }
    memset(ram->cells, 0, sizeof(ram->cells));
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    // Initialize the read-write lock with default attributes.
    pthread_rwlock_init(&ram->lock, &attr);
    // Destroy the temporary attributes object.
    pthread_rwlockattr_destroy(&attr);
    log_write(LOG_INFO, "RAM initialized. Processed %u memory cells", RAM_SIZE);
}

/**
 * @brief Store a 32-bit value at the specified RAM address.
 *
 * Validates the input parameters and uses an exclusive write lock to
 * serialize writers and protect the shared RAM cells array.
 *
 * @param ram Pointer to an initialized Ram instance.
 * @param address Target address within RAM where the value will be stored.
 * @param value The 32-bit value to store.
 * @return true on success, false on error (NULL ram pointer or invalid address).
 *
 * @thread_safety This function acquires an exclusive write lock (pthread
 * rwlock) for the duration of the write, blocking other readers and writers.
 *
 * @sideeffects Logs errors and debug messages via log_write.
 */
bool ram_store(RAM *ram, const uint32_t address, const uint32_t value) {
    if (!ram) {
        log_write(LOG_ERROR, "RAM store failed: RAM pointer is NULL");
        return false;
    }

    if (!is_address_valid(ram, address)) {
        log_write(LOG_ERROR, "RAM store failed: Invalid address 0x%X", (unscast) address);
        return false;
    }

    // Acquire exclusive write lock. Blocks until no readers or writers hold the lock.
    pthread_rwlock_wrlock(&ram->lock);
    // Critical section: single-element write
    ram->cells[address] = value;
    // Release the lock as soon as possible
    pthread_rwlock_unlock(&ram->lock);

    /*
     * Logging performed after unlocking to avoid holding the lock during
     * potentially slow operations.
     */
    log_write(LOG_DEBUG, "[RAM STORE] Writing inside address 0x%04x value %u", (unscast) address, (unscast) value);
    return true;
}

/**
 * @brief Load a 32-bit value from the specified RAM address.
 *
 * Validates parameters and uses a shared read lock to allow concurrent
 * readers while protecting against concurrent writers.
 *
 * @param ram Pointer to an initialized Ram instance.
 * @param address Address to read from (must be within RAM bounds).
 * @param output Pointer to an uint32_t where the read value will be stored.
 * Must be non-NULL.
 * @return true on success (and *output is set), false on error.
 *
 * @thread_safety This function acquires a shared read lock (pthread rwlock),
 * allowing multiple simultaneous readers but blocking writers.
 *
 * @sideeffects Logs errors and debug messages via log_write.
 */
bool ram_load(RAM *ram, const uint32_t address, uint32_t *output) {
    if (!ram || output == NULL) {
        log_write(LOG_ERROR, "Ram load failed: Ram or output arguments are NULL");
        return false;
    }

    if (!is_address_valid(ram, address)) {
        log_write(LOG_ERROR, "Ram load failed: Attempted to read outside of RAM bounds at address 0x%04x", (unscast) address);
        return false;
    }

    // Acquire shared read lock to allow multiple simultaneous readers.
    pthread_rwlock_rdlock(&ram->lock);
    const uint32_t val = ram->cells[address];
    // Release the read lock immediately to minimize contention.
    pthread_rwlock_unlock(&ram->lock);

    // Write result to caller and log outside the lock.
    *output = val;
    log_write(LOG_DEBUG, "[RAM LOAD] Reading inside address 0x%04x value %u", (unscast) address, (unscast) *output);
    return true;
}

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
bool ram_free(RAM *ram, uint32_t start, uint32_t end) {
    if (!ram) {
        log_write(LOG_ERROR, "RAM free failed: RAM pointer is NULL");
        return false;
    }

    if (!is_address_valid(ram, start) || !is_address_valid(ram, end) || start > end) {
        log_write(LOG_ERROR, "RAM free failed: Invalid range 0x%04X to 0x%04X", (unscast) start, (unscast) end);
        return false;
    }

    int rc = pthread_rwlock_wrlock(&ram->lock);
    if (rc != 0) {
        log_write(LOG_ERROR, "RAM free failed: could not acquire write lock (%d)", rc);
        return false;
    }

    size_t count = (size_t) (end - start + 1);
    memset(&ram->cells[start], 0, count * sizeof(ram->cells[0]));

    rc = pthread_rwlock_unlock(&ram->lock);
    if (rc != 0) {
        log_write(LOG_ERROR, "RAM free warning: failed to release write lock (%d)", rc);
    }

    log_write(LOG_INFO, "RAM free: Cleared range 0x%04X to 0x%04X", (unscast) start, (unscast) end);
    return true;
}