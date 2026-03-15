/**
 * @file   kiln_hal.h
 * @brief  Platform abstraction layer for Kiln tile drivers.
 *
 * Provides a hardware-independent interface for I2C communication and
 * timing. Each tile driver accepts a kiln_hal_t pointer instead of a
 * platform-specific handle, making drivers portable across MCU families.
 *
 * To port Kiln to a new platform, implement the three function pointers
 * in kiln_hal_t for your target's I2C and delay primitives. See
 * kiln_hal_stm32.c for the STM32 HAL reference implementation.
 */

#ifndef INC_KILN_HAL_H_
#define INC_KILN_HAL_H_

#include <stdint.h>

/**
 * @brief  Platform abstraction handle.
 *
 * Wraps an opaque platform handle (e.g. an I2C peripheral) with portable
 * function pointers for register-level I2C access and millisecond delays.
 *
 * Usage:
 * @code
 *   // STM32 example — see kiln_hal_stm32.c for helper
 *   kiln_hal_t hal;
 *   kiln_hal_stm32_init(&hal, &hi2c1);
 *
 *   tile_sense_i_9_init(&hal);
 * @endcode
 */
typedef struct {
    /**
     * @brief  Read one or more bytes from a device register.
     *
     * @param  handle   Opaque platform handle (stored in kiln_hal_t.handle)
     * @param  addr     7-bit I2C device address (unshifted)
     * @param  reg      Register address to read from
     * @param  data     Output buffer
     * @param  len      Number of bytes to read
     * @return 0 on success, non-zero on error
     */
    int (*i2c_read)(void* handle, uint8_t addr, uint8_t reg,
                    uint8_t* data, uint16_t len);

    /**
     * @brief  Write one or more bytes to a device register.
     *
     * @param  handle   Opaque platform handle (stored in kiln_hal_t.handle)
     * @param  addr     7-bit I2C device address (unshifted)
     * @param  reg      Register address to write to
     * @param  data     Input buffer
     * @param  len      Number of bytes to write
     * @return 0 on success, non-zero on error
     */
    int (*i2c_write)(void* handle, uint8_t addr, uint8_t reg,
                     const uint8_t* data, uint16_t len);

    /**
     * @brief  Check whether a device acknowledges its address.
     *
     * @param  handle   Opaque platform handle
     * @param  addr     7-bit I2C device address (unshifted)
     * @return 0 if device is present, non-zero otherwise
     */
    int (*i2c_is_ready)(void* handle, uint8_t addr);

    /**
     * @brief  Blocking delay in milliseconds.
     *
     * @param  ms  Number of milliseconds to wait
     */
    void (*delay_ms)(uint32_t ms);

    /** @brief  Opaque platform handle passed to all callbacks. */
    void* handle;
} kiln_hal_t;

#endif /* INC_KILN_HAL_H_ */
