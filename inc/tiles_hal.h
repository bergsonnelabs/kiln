/**
 * @file   tiles_hal.h
 * @brief  Platform abstraction layer for tile drivers.
 *
 * Provides a hardware-independent interface for bus communication (I2C,
 * SPI, QSPI), timing, and optional error reporting. Each tile driver
 * accepts a tiles_hal_t pointer instead of a platform-specific handle,
 * making drivers portable across MCU families and frameworks.
 *
 * To port to a new platform, create a cfg struct and init function
 * that populates the tiles_hal_t function pointers for your target's
 * bus and delay primitives. See the HAL/ directory for ready-to-use
 * implementations (Arduino, ESP-IDF, STM32).
 *
 * Usage:
 * @code
 *   tiles_hal_stm32_cfg_t cfg = {
 *       .i2c = &hi2c1,
 *       .buses = TILES_BUS_I2C,
 *   };
 *   tiles_hal_t hal;
 *   tiles_hal_stm32_init(&hal, &cfg);
 *
 *   tile_t imu = tile_sense_i_9_init(&hal, 0);
 * @endcode
 */

#ifndef INC_TILES_HAL_H_
#define INC_TILES_HAL_H_

#include <stdint.h>

/* Forward declaration — tile_t is defined in tiles.h */
struct tile;

/* ------------------------------------------------------------------ */
/*  Bus type flags                                                    */
/* ------------------------------------------------------------------ */

#define TILES_BUS_I2C   (1 << 0)   /**< I2C bus available   */
#define TILES_BUS_SPI   (1 << 1)   /**< SPI bus available   */
#define TILES_BUS_QSPI  (1 << 2)   /**< QSPI bus available  */
#define TILES_BUS_I3C   (1 << 3)   /**< I3C bus available   */

/* ------------------------------------------------------------------ */
/*  Platform abstraction handle                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief  Platform abstraction handle.
 *
 * Wraps opaque platform handles with portable function pointers for
 * register-level bus access, millisecond delays, and optional error
 * reporting. Populate only the buses your project uses — leave unused
 * pointers NULL.
 */
typedef struct {
    /* --- I2C --- */

    /**
     * @brief  Read one or more bytes from a device register via I2C.
     *
     * @param  handle   Opaque platform handle (stored in tiles_hal_t.handle)
     * @param  addr     7-bit I2C device address (unshifted)
     * @param  reg      Register address to read from
     * @param  data     Output buffer
     * @param  len      Number of bytes to read
     * @return 0 on success, non-zero on error
     */
    int (*i2c_read)(void* handle, uint8_t addr, uint8_t reg,
                    uint8_t* data, uint16_t len);

    /**
     * @brief  Write one or more bytes to a device register via I2C.
     *
     * @param  handle   Opaque platform handle (stored in tiles_hal_t.handle)
     * @param  addr     7-bit I2C device address (unshifted)
     * @param  reg      Register address to write to
     * @param  data     Input buffer
     * @param  len      Number of bytes to write
     * @return 0 on success, non-zero on error
     */
    int (*i2c_write)(void* handle, uint8_t addr, uint8_t reg,
                     const uint8_t* data, uint16_t len);

    /**
     * @brief  Check whether a device acknowledges its address on I2C.
     *
     * @param  handle   Opaque platform handle
     * @param  addr     7-bit I2C device address (unshifted)
     * @return 0 if device is present, non-zero otherwise
     */
    int (*i2c_is_ready)(void* handle, uint8_t addr);

    /* --- SPI --- */

    /**
     * @brief  Read one or more bytes from a device register via SPI.
     *
     * @param  handle   Opaque platform handle
     * @param  cs       Chip-select identifier (pin number or index)
     * @param  reg      Register address to read from (read bit applied by impl)
     * @param  data     Output buffer
     * @param  len      Number of bytes to read
     * @return 0 on success, non-zero on error
     */
    int (*spi_read)(void* handle, uint8_t cs, uint8_t reg,
                    uint8_t* data, uint16_t len);

    /**
     * @brief  Write one or more bytes to a device register via SPI.
     *
     * @param  handle   Opaque platform handle
     * @param  cs       Chip-select identifier (pin number or index)
     * @param  reg      Register address to write to
     * @param  data     Input buffer
     * @param  len      Number of bytes to write
     * @return 0 on success, non-zero on error
     */
    int (*spi_write)(void* handle, uint8_t cs, uint8_t reg,
                     const uint8_t* data, uint16_t len);

    /* --- Shared --- */

    /**
     * @brief  Blocking delay in milliseconds.
     *
     * @param  ms  Number of milliseconds to wait
     */
    void (*delay_ms)(uint32_t ms);

    /**
     * @brief  Optional error callback — set to NULL to silence.
     *
     * When a driver detects an error (wrong state, bus failure, etc.),
     * it calls this if non-NULL. Route to printf, UART, BLE, etc.
     *
     * @param  inst  The tile instance that triggered the error
     * @param  msg   Human-readable description
     */
    void (*on_error)(struct tile t, const char* msg);

    /** @brief  Opaque platform handle passed to I2C callbacks. */
    void* handle;

    /** @brief  Active bus flags (TILES_BUS_I2C, TILES_BUS_SPI, etc.). */
    uint8_t buses;
} tiles_hal_t;

#endif /* INC_TILES_HAL_H_ */
