/**
 * @file   tiles_hal_core.h
 * @brief  Core tile HAL — for use when a Core tile is the host platform.
 *
 * Provides a HAL init for projects running on Mosaic Core tiles.
 * All Core tiles currently use STM32 processors, so this wraps the
 * STM32 HAL internally. As Core expands to other MCU families, this
 * header remains the same — only the implementation changes.
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
 *   tiles_hal_core_cfg_t cfg = {
 *       .i2c = &hi2c1,
 *       .buses = TILES_BUS_I2C,
 *   };
 *   tiles_hal_t hal;
 *   tiles_hal_core_init(&hal, &cfg);
 *
 *   tile_t imu = tile_sense_i_9_init(&hal, 0);
 * @endcode
 */

#ifndef TILES_HAL_CORE_H_
#define TILES_HAL_CORE_H_

#include "tiles.h"

/* Pull in the right STM32 HAL header based on the Core tile variant */
#if defined(CORE_W) || defined(STM32WBxx)
#include "stm32wbxx_hal.h"
#elif defined(CORE_H) || defined(STM32H5xx)
#include "stm32h5xx_hal.h"
#elif defined(CORE_U) || defined(STM32L4xx)
#include "stm32l4xx_hal.h"
#elif defined(CORE_L) || defined(STM32L0xx)
#include "stm32l0xx_hal.h"
#else
/* Fallback — your project must provide the correct HAL include */
#endif

/**
 * @brief  Core tile HAL configuration.
 *
 * Set the bus handles for the peripherals your Core tile exposes.
 * Bus peripherals must be initialized before calling tiles_hal_core_init.
 *
 * Note: Core tiles have fixed I2C/SPI peripheral assignments per variant.
 * Refer to the Core tile documentation for which peripheral maps to which pads.
 */
typedef struct {
    I2C_HandleTypeDef*  i2c;              /**< I2C peripheral handle             */
    SPI_HandleTypeDef*  spi;              /**< SPI peripheral handle (if used)   */
    GPIO_TypeDef*       spi_cs_ports[8];  /**< GPIO port for each CS line        */
    uint16_t            spi_cs_pins[8];   /**< GPIO pin for each CS line         */
    uint8_t             buses;            /**< TILES_BUS_I2C | TILES_BUS_SPI     */
} tiles_hal_core_cfg_t;

/**
 * @brief  Initialize a tiles_hal_t for a Core tile.
 *
 * @param  hal  Pointer to tiles_hal_t to populate
 * @param  cfg  Core-specific configuration
 */
void tiles_hal_core_init(tiles_hal_t* hal, const tiles_hal_core_cfg_t* cfg);

#endif /* TILES_HAL_CORE_H_ */
