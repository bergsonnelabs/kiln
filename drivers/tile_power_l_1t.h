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
 * @tessera tile label=Power.L.1T icon=☷
 *
 * Quick start:
 * @code
 *   #include "core_tiles.h"
 *
 *   tile_t battery;
 *   tile_power_l_1t_init(core_tiles_pal(&core_i2c1), 0, &battery, NULL);
 *   if (tile_is_ready(&battery)) {
 *       uint16_t vbat = tile_power_l_1t_get_vbat(&battery);
 *       uint8_t  pct  = tile_power_l_1t_get_percent(&battery);
 *   }
 * @endcode
 *
 * Driver gaps (chip capabilities not exposed by this driver):
 *
 * The current driver is intentionally minimal (vbat read + state-of-
 * charge derivation) and surfaces only a fraction of the BQ25150's
 * capability surface. The gaps below are slated for the next driver
 * pass; the watchdog gap in particular is a correctness issue, not
 * just a feature deferral.
 *
 * @tessera unsupported severity=common category="I²C watchdog feed"
 *   BQ25150 has a 50-second I²C watchdog that resets ALL charge
 *   parameters (ICHG, VBATREG, ITERM, ILIM, etc.) to defaults if
 *   the host doesn't periodically write to a charger register.
 *   Driver doesn't kick the watchdog — meaning any custom charge
 *   configuration is silently lost every 50 s. Correctness bug,
 *   not just a missing feature.
 *
 * @tessera unsupported severity=common category="Charge current programming (ICHG_CTRL)"
 *   Programmable 1.25–500 mA in 10 mA steps via ICHG_CTRL. Driver
 *   uses chip default (typically 200 mA) — required for matching
 *   battery's 0.5C–1C charge spec.
 *
 * @tessera unsupported severity=common category="Charge voltage programming (VBATREG)"
 *   Programmable 3.6–4.6 V in 10 mV steps via VBAT_CTRL. Driver
 *   uses chip default (4.2 V) — relevant for LFP cells (3.65 V)
 *   or high-voltage NMC variants.
 *
 * @tessera unsupported severity=common category="Pre-charge / termination current"
 *   PCHRGCTRL (1.25–77.5 mA pre-charge) and TERMCTRL (1–31 % of
 *   ICHARGE termination) aren't exposed. Pre-charge prevents
 *   damage to deeply-discharged cells; termination current
 *   determines when CV phase ends.
 *
 * @tessera unsupported severity=common category="ADC channels (VIN / ICHG / TS / ADCIN)"
 *   12-bit ADC measures input voltage, charge current, NTC
 *   thermistor, and an optional ADCIN. Driver only reads VBAT —
 *   the rest are needed for proper power-path diagnostics.
 *
 * @tessera unsupported severity=common category="NTC thresholds (TS_COLD/COOL/WARM/HOT)"
 *   Programmable JEITA-style temperature thresholds gate charging
 *   based on battery temperature via the TS pin. Driver doesn't
 *   configure them — relevant for battery safety in any
 *   non-room-temperature application.
 *
 * @tessera unsupported severity=common category="Fault flag readout"
 *   FLAG0–FLAG3 registers report charge faults (BAT_OCP, VIN_OVP,
 *   UVLO, safety timer, watchdog, OTS, etc.). Driver has
 *   read_status as a register-level escape hatch but no
 *   structured fault API.
 *
 * @tessera unsupported severity=advanced category="Ship mode entry / exit"
 *   EN_SHIPMODE bit puts the chip into ~10 nA quiescent (battery
 *   physically disconnected via internal FET); waking requires MR
 *   press or VIN. Driver doesn't expose ship-mode toggling —
 *   relevant for long-term storage or end-of-line testing.
 *
 * @tessera unsupported severity=advanced category="LDO enable + voltage selection"
 *   LDO output (1.8 V default on this tile, programmable 0.6–3.7 V
 *   via LDOCTRL) and load-switch mode aren't controlled by the
 *   driver. EN_LS_LDO bit, soft-start, and current-limit reporting
 *   would help apps that gate downstream rails.
 *
 * @tessera unsupported severity=advanced category="MR button + INT pin handling"
 *   MR push-button input supports short / long press detection
 *   (configurable WAKE1/WAKE2 timers) and the INT pin pulses on
 *   any flag change (with maskable sources). Driver has no
 *   button / interrupt API.
 *
 * @tessera unsupported severity=advanced category="14 s HW watchdog control"
 *   Separate from the I²C watchdog: a 14 s VIN-stable HW watchdog
 *   triggers a Power-Cycle/Autowake reset if no I²C activity. Can
 *   be disabled via HWRESET_14S_WD bit. Driver doesn't expose the
 *   bit — the HW watchdog stays enabled by default.
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
 * Optional init config. Pass NULL for defaults.
 * Reserved for future use.
 */
typedef struct {
    uint8_t reserved;   /**< Placeholder — no options yet. */
} power_l_1t_cfg_t;

/**
 * @brief  Initialize the BQ25150 charge controller.
 *
 * Verifies the device ID and configures charge current, precharge,
 * undervoltage lockout, ADC, and disables ship mode. Pass cfg=NULL for defaults.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @param  tile      Pointer to tile handle (populated by this function)
 * @param  cfg       Optional config, or NULL for defaults
 */
void tile_power_l_1t_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                          const power_l_1t_cfg_t *cfg);

/**
 * @brief  Read the raw battery voltage from the ADC.
 *
 * @tessera expose category=tile name=get_vbat returns=int
 * @param  tile  Pointer to tile handle
 * @return 16-bit raw ADC value (MSB:LSB)
 */
uint16_t tile_power_l_1t_get_vbat(tile_t* tile);

/**
 * @brief  Read the battery charge percentage.
 *
 * @tessera expose category=tile name=get_percent returns=int
 * @param  tile  Pointer to tile handle
 * @return Battery percentage (0–100)
 */
uint8_t tile_power_l_1t_get_percent(tile_t* tile);

/**
 * @brief  Read a status register.
 *
 * @tessera expose category=tile name=read_status returns=int
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
