/**
 * @file   tile_power_l_1t.h
 * @brief  Li-Ion charge controller driver for the Power.L.1T tile (rev a).
 *
 * Embeds the Texas Instruments BQ25150, a single-cell Li-Ion charge
 * controller with programmable 1.8 V LDO output and 12-bit ADC
 * for battery and system monitoring.
 *
 * Key specifications:
 *   - Charge input:     3.4–5.5 V (up to 500 mA)
 *   - Battery voltage:  3.6–4.6 V (rechargeable Li-Ion)
 *   - LDO output:       1.8 V, up to 10 mA
 *   - ADC:              12-bit battery voltage monitoring
 *   - Charge current:   programmable, up to 500 mA
 *
 * Datasheet: https://www.bergsonne.io/tiles/power/l1t
 * IC datasheet: https://www.ti.com/lit/ds/symlink/bq25150.pdf
 *
 * Quick start:
 * @code
 *   #include "core_tiles.h"
 *
 *   tile_t battery;
 *   tile_power_l_1t_init(core_tiles_pal(&core_i2c1), 0, &battery);
 *   if (tile_is_ready(&battery)) {
 *       uint16_t vbat = tile_power_l_1t_get_vbat(&battery);
 *       uint8_t  pct  = tile_power_l_1t_get_percent(&battery);
 *   }
 * @endcode
 */

#ifndef INC_TILE_POWER_L_1T_H_
#define INC_TILE_POWER_L_1T_H_

#include "tiles.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* Driver version                                                  */
/* -------------------------------------------------------------- */

#define TILE_POWER_L_1T_VERSION_MAJOR  2
#define TILE_POWER_L_1T_VERSION_MINOR  0
#define TILE_POWER_L_1T_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);  /* requires tiles.h >= 1.0 */

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

/**
 * @brief  Instance-to-address mapping for Power.L.1T.
 *
 * | Instance | ID   | Bus  | Hardware config      |
 * |----------|------|------|----------------------|
 * | 0        | 0x6B | I2C  | Fixed address        |
 *
 * @note  The BQ25150 has a single fixed I2C address. Multiple
 *        Power.L.1T tiles require separate I2C buses.
 */
#define BQ25150_I2C_ADDR_DEFAULT    0x6B

/* -------------------------------------------------------------- */
/* BQ25150 register map (subset used by this driver)               */
/* -------------------------------------------------------------- */

#define BQ25150_REG_STAT0           0x00  /**< Charger status 0 */
#define BQ25150_REG_STAT1           0x01  /**< Charger status 1 */
#define BQ25150_REG_STAT2           0x02  /**< Charger status 2 */
#define BQ25150_REG_FLAG0           0x03  /**< Flag register 0 */
#define BQ25150_REG_FLAG1           0x04  /**< Flag register 1 */
#define BQ25150_REG_FLAG2           0x05  /**< Flag register 2 */
#define BQ25150_REG_FLAG3           0x06  /**< Flag register 3 */
#define BQ25150_REG_ICHG_CTRL       0x13  /**< Fast charge current control */
#define BQ25150_REG_PCHRGCTRL       0x14  /**< Precharge current control */
#define BQ25150_REG_BUVLO           0x16  /**< Battery undervoltage lockout */
#define BQ25150_REG_ICCTRL0         0x35  /**< IC control 0 (ship mode) */
#define BQ25150_REG_ADCCTRL0        0x40  /**< ADC control 0 */
#define BQ25150_REG_VBAT_MSB        0x42  /**< Battery voltage MSB */
#define BQ25150_REG_VBAT_LSB        0x43  /**< Battery voltage LSB */
#define BQ25150_REG_ADC_READ_EN     0x58  /**< ADC channel read enable */
#define BQ25150_REG_DEVICE_ID       0x6F  /**< Device identification */

/** @brief  Expected DEVICE_ID register value. */
#define BQ25150_DEVICE_ID_DEFAULT   0x20

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

/**
 * @brief  Check whether a BQ25150 is present on the I2C bus.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_power_l_1t_find(tiles_pal_t* hal, uint8_t instance);

/**
 * @brief  Initialize the BQ25150 charge controller.
 *
 * Verifies the device ID and configures charge current, precharge,
 * undervoltage lockout, ADC, and disables ship mode.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @param  tile      Pointer to tile handle (populated by this function)
 */
void tile_power_l_1t_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile);

/**
 * @brief  Read the raw battery voltage from the ADC.
 *
 * @param  tile  Pointer to tile handle
 * @return 16-bit raw ADC value (MSB:LSB)
 */
uint16_t tile_power_l_1t_get_vbat(tile_t* tile);

/**
 * @brief  Read the battery charge percentage.
 *
 * @param  tile  Pointer to tile handle
 * @return Battery percentage (0–100)
 */
uint8_t tile_power_l_1t_get_percent(tile_t* tile);

/**
 * @brief  Read a status register.
 *
 * @param  tile  Pointer to tile handle
 * @param  reg   Status register address (BQ25150_REG_STAT0/1/2)
 * @return 8-bit register value
 */
uint8_t tile_power_l_1t_read_status(tile_t* tile, uint8_t reg);

/**
 * @brief  Write a raw 8-bit value to any BQ25150 register.
 *
 * @param  tile   Pointer to tile handle
 * @param  reg    8-bit register address
 * @param  value  8-bit value to write
 */
void tile_power_l_1t_write_reg(tile_t* tile, uint8_t reg, uint8_t value);

#endif /* INC_TILE_POWER_L_1T_H_ */
