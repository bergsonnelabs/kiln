/**
 * @file   tile_power_l_1t.h
 * @brief  Li-Ion charge controller driver for the Power.L.1T tile (rev a).
 *
 * Datasheet: https://www.bergsonne.io/tiles/power/l1t
 * JSON API:  https://www.bergsonne.io/api/tile-json?family=Power&name=L.1T&rev=a
 *
 * The Power.L.1T embeds the Texas Instruments BQ25150, a single-cell
 * Li-Ion charge controller with programmable 1.8 V LDO output and
 * 12-bit ADC for battery and system monitoring.
 *
 * Key specifications:
 *   - Charge input:     3.4–5.5 V (up to 500 mA)
 *   - Battery voltage:  3.6–4.6 V (rechargeable Li-Ion)
 *   - LDO output:       1.8 V, up to 10 mA
 *   - ADC:              12-bit battery voltage monitoring
 *   - Charge current:   programmable, up to 500 mA
 *   - Ship mode:        ultra-low-current storage mode
 *
 * IC datasheet:
 *   https://www.ti.com/lit/ds/symlink/bq25150.pdf
 *
 * Tile hardware (T44-10 package, 4.0 × 4.0 mm):
 *   - Pad 1:   GND
 *   - Pad 2:   LP — drive >1.35 V to exit low-power mode
 *   - Pad 3:   SW — connect to GND to disable LDO output
 *   - Pad 4:   I2C.CLK (external pull-up required without Core)
 *   - Pad 5:   I2C.DAT (external pull-up required without Core)
 *   - Pad 6:   BATT−
 *   - Pad 7:   BATT+
 *   - Pad 8:   SUPPLY+ (3.15–5.5 V, 500 mA max)
 *   - Pad 9:   SUPPLY−
 *   - Pad 10:  V+ — 1.8 V LDO output (10 mA max)
 *
 * I2C: 7-bit address, 8-bit register, 8-bit data.
 *
 * Requires: kiln_hal.h platform abstraction.
 *
 * Quick start:
 * @code
 *   kiln_hal_t hal;
 *   kiln_hal_stm32_init(&hal, &hi2c1);
 *
 *   if (tile_power_l_1t_init(&hal, BQ25150_I2C_ADDR_DEFAULT)) {
 *       uint16_t vbat_raw = tile_power_l_1t_get_vbat();
 *       uint8_t  percent  = tile_power_l_1t_get_percent();
 *   }
 * @endcode
 */

#ifndef INC_TILE_POWER_L_1T_H_
#define INC_TILE_POWER_L_1T_H_

#include "kiln_hal.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* I2C addresses                                                   */
/* -------------------------------------------------------------- */

/** @brief  Default I2C address (7-bit). */
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
 * Performs an address-level probe only (no register reads).
 *
 * @param  hal   Platform HAL handle
 * @param  addr  7-bit I2C address (BQ25150_I2C_ADDR_DEFAULT)
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_power_l_1t_find(kiln_hal_t* hal, uint8_t addr);

/**
 * @brief  Initialize the BQ25150 charge controller.
 *
 * Verifies the device ID and configures:
 *   - Fast charge current: 80 mA
 *   - Precharge current: 20 mA
 *   - Battery undervoltage lockout: 2.6 V
 *   - ADC: VBAT channel enabled, 1-second update rate
 *   - Ship mode: disabled
 *
 * The selected HAL and address are stored internally for all
 * subsequent calls.
 *
 * @param  hal   Platform HAL handle
 * @param  addr  7-bit I2C address (BQ25150_I2C_ADDR_DEFAULT)
 * @return 1 on success, 0 if device ID check fails
 */
uint8_t tile_power_l_1t_init(kiln_hal_t* hal, uint8_t addr);

/**
 * @brief  Switch the active HAL and address context.
 *
 * When multiple Power.L.1T tiles share a driver, call this to
 * redirect all subsequent API calls to a different tile without
 * re-running the full init sequence.
 *
 * @param  hal   Platform HAL handle for the target bus
 * @param  addr  7-bit I2C address of the target device
 */
void tile_power_l_1t_select(kiln_hal_t* hal, uint8_t addr);

/**
 * @brief  Read the raw battery voltage from the ADC.
 *
 * Returns the 12-bit ADC value from the VBAT_MSB and VBAT_LSB
 * registers. The voltage in millivolts is approximately:
 *   voltage_mV = raw × (5000.0 / 65535.0) × 1000
 *
 * @return 16-bit raw ADC value (MSB:LSB)
 */
uint16_t tile_power_l_1t_get_vbat(void);

/**
 * @brief  Read the battery charge percentage.
 *
 * Converts the raw ADC voltage to a 0–100% value using a linear
 * approximation based on typical Li-Ion discharge curves.
 *
 * @return Battery percentage (0–100)
 */
uint8_t tile_power_l_1t_get_percent(void);

/**
 * @brief  Read a status register.
 *
 * @param  reg  Status register address (BQ25150_REG_STAT0/1/2)
 * @return 8-bit register value
 */
uint8_t tile_power_l_1t_read_status(uint8_t reg);

/**
 * @brief  Write a raw 8-bit value to any BQ25150 register.
 *
 * Low-level register access for advanced configuration.
 *
 * @param  reg    8-bit register address
 * @param  value  8-bit value to write
 */
void tile_power_l_1t_write_reg(uint8_t reg, uint8_t value);

#endif /* INC_TILE_POWER_L_1T_H_ */
