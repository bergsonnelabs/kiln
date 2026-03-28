/**
 * @file   tiles_hal_core.h
 * @brief  Core tile HAL — native implementation using the Cores SDK.
 *
 * Provides a HAL init for projects running on Mosaic Core tiles,
 * using the Cores firmware SDK's LL/HAL layer directly.  No CubeIDE
 * HAL dependency — just our lightweight register-level drivers.
 *
 * Supported Core tiles:
 *   - Core.L  — STM32L011 (Cortex-M0+)
 *   - Core.U  — STM32L422 (Cortex-M4)
 *   - Core.W  — STM32WBA55 (Cortex-M33, BLE 5.4)
 *   - Core.H  — STM32H523 (Cortex-M33, 250 MHz)
 *
 * Usage:
 * @code
 *   #include "tiles.h"
 *   #include "tiles_hal_core.h"
 *   #include "tile_sense_i_9.h"
 *
 *   // Assumes I2C1 and SPI1 are initialized via tile_init() / project.json
 *   tiles_hal_core_cfg_t cfg = {
 *       .i2c = &my_i2c,          // hal_i2c_t from Cores SDK
 *       .buses = TILES_BUS_I2C,
 *   };
 *   tiles_hal_t hal;
 *   tiles_hal_core_init(&hal, &cfg);
 *
 *   tile_t imu;
 *   tile_sense_i_9_init(&hal, 0, &imu);
 * @endcode
 */

#ifndef TILES_HAL_CORE_H_
#define TILES_HAL_CORE_H_

#include "tiles.h"

/* Cores SDK HAL types — forward declared to avoid pulling in
   the full Cores headers into every driver. */
typedef struct hal_i2c hal_i2c_t;
typedef struct hal_spi hal_spi_t;

/* GPIO type for SPI chip-select — matches Cores SDK ll_common.h */
typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFRL;
    volatile uint32_t AFRH;
} tiles_gpio_t;

/**
 * @brief  SPI chip-select descriptor.
 *
 * Each SPI tile needs a CS pin.  The index in the cs[] array
 * matches the instance number used in driver init calls.
 */
typedef struct {
    tiles_gpio_t*  port;    /**< GPIO port (e.g., GPIOA)   */
    uint32_t       pin;     /**< Pin number (0–15)         */
} tiles_hal_core_cs_t;

/**
 * @brief  Core tile HAL configuration.
 *
 * Set the bus handles for the peripherals your Core tile exposes.
 * Bus peripherals must be initialized (via tile_init / project.json
 * or manual hal_i2c_init / hal_spi_init) before calling
 * tiles_hal_core_init.
 */
typedef struct {
    hal_i2c_t*           i2c;       /**< Cores SDK I2C handle          */
    hal_spi_t*           spi;       /**< Cores SDK SPI handle          */
    tiles_hal_core_cs_t  cs[8];     /**< SPI chip-select pins          */
    uint8_t              buses;     /**< TILES_BUS_I2C | TILES_BUS_SPI */
} tiles_hal_core_cfg_t;

/**
 * @brief  Initialize a tiles_hal_t for a Core tile.
 *
 * @param  hal  Pointer to tiles_hal_t to populate
 * @param  cfg  Core-specific configuration
 */
void tiles_hal_core_init(tiles_hal_t* hal, const tiles_hal_core_cfg_t* cfg);

#endif /* TILES_HAL_CORE_H_ */
