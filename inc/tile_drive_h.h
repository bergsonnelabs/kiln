/**
 * @file   tile_drive_h.h
 * @brief  LRA haptic driver for the Drive.H tile (rev a).
 *
 * Datasheet: https://www.bergsonne.io/tiles/drive/h
 *
 * The Drive.H embeds the TI DRV2605L, a haptic driver for LRA
 * (Linear Resonant Actuator) and ERM actuators with a built-in
 * waveform library of 123 effects.
 *
 * Key specifications:
 *   - Output:       full-bridge, 3.0–5.2 Vrms into LRA
 *   - Waveform lib: 123 haptic effects (6 libraries)
 *   - Auto-cal:     automatic resonance tracking for LRA
 *   - Modes:        Internal trigger, RTP, PWM, audio-to-vibe
 *
 * IC datasheet:
 *   https://www.ti.com/lit/ds/symlink/drv2605l.pdf
 *
 * I2C: 7-bit address 0x5A, 8-bit register, 8-bit data.
 *
 * Requires: kiln_hal.h platform abstraction.
 *
 * Quick start:
 * @code
 *   kiln_hal_t hal;
 *   kiln_hal_stm32_init(&hal, &hi2c1);
 *
 *   if (tile_drive_h_init(&hal, DRV2605L_I2C_ADDR_DEFAULT)) {
 *       tile_drive_h_play(1, 1);  // play effect #1 once
 *   }
 * @endcode
 */

#ifndef INC_TILE_DRIVE_H_H_
#define INC_TILE_DRIVE_H_H_

#include "kiln_hal.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* I2C addresses                                                   */
/* -------------------------------------------------------------- */

/** @brief  Default I2C address (7-bit). */
#define DRV2605L_I2C_ADDR_DEFAULT   0x5A

/* -------------------------------------------------------------- */
/* DRV2605L register map                                           */
/* -------------------------------------------------------------- */

#define DRV2605L_REG_STATUS         0x00  /**< Status register */
#define DRV2605L_REG_MODE           0x01  /**< Mode register */
#define DRV2605L_REG_LIBRARY_SEL    0x03  /**< Waveform library selection */
#define DRV2605L_REG_WAVE_SEQ_0     0x04  /**< Waveform sequence slot 0 */
#define DRV2605L_REG_WAVE_SEQ_1     0x05  /**< Waveform sequence slot 1 */
#define DRV2605L_REG_WAVE_SEQ_2     0x06  /**< Waveform sequence slot 2 */
#define DRV2605L_REG_WAVE_SEQ_3     0x07  /**< Waveform sequence slot 3 */
#define DRV2605L_REG_GO             0x0C  /**< Go register (trigger playback) */
#define DRV2605L_REG_RTP            0x02  /**< Real-time playback input */
#define DRV2605L_REG_RATED_VOLTAGE  0x16  /**< Rated voltage for actuator */
#define DRV2605L_REG_OD_CLAMP      0x17  /**< Overdrive clamp voltage */
#define DRV2605L_REG_FEEDBACK_CTRL  0x1A  /**< Feedback control register */
#define DRV2605L_REG_CONTROL3       0x1D  /**< Control3 register */

/** @brief  Expected default STATUS register value. */
#define DRV2605L_STATUS_DEFAULT     0x60

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

/**
 * @brief  Check whether a DRV2605L is present on the I2C bus.
 *
 * Performs an address-level probe only (no register reads).
 *
 * @param  hal   Platform HAL handle
 * @param  addr  7-bit I2C address (DRV2605L_I2C_ADDR_DEFAULT)
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_drive_h_find(kiln_hal_t* hal, uint8_t addr);

/**
 * @brief  Initialize the DRV2605L haptic driver.
 *
 * Verifies the status register, exits standby, and configures
 * for LRA open-loop operation with library 6.
 *
 * Note: if the Drive.H has an enable pin, the application must
 * assert it before calling this function.
 *
 * @param  hal   Platform HAL handle
 * @param  addr  7-bit I2C address (DRV2605L_I2C_ADDR_DEFAULT)
 * @return 1 on success, 0 if status check fails
 */
uint8_t tile_drive_h_init(kiln_hal_t* hal, uint8_t addr);

/**
 * @brief  Switch the active HAL and address context.
 *
 * When multiple Drive.H tiles share a driver, call this to
 * redirect all subsequent API calls to a different tile.
 *
 * @param  hal   Platform HAL handle for the target bus
 * @param  addr  7-bit I2C address of the target device
 */
void tile_drive_h_select(kiln_hal_t* hal, uint8_t addr);

/**
 * @brief  Play a waveform effect from the built-in library.
 *
 * Loads the effect index into sequence slot 0, terminates
 * the sequence, and triggers playback. For repeats > 1,
 * re-triggers with a 200ms gap between plays.
 *
 * @param  index    Library effect index (1–123, see datasheet)
 * @param  repeats  Number of times to play (1 = once)
 */
void tile_drive_h_play(uint8_t index, uint8_t repeats);

/**
 * @brief  Stop any currently playing effect.
 *
 * Writes 0 to the GO register.
 */
void tile_drive_h_stop(void);

#endif /* INC_TILE_DRIVE_H_H_ */
