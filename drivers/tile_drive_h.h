/**
 * @file   tile_drive_h.h
 * @brief  LRA haptic driver for the Drive.H tile (rev a).
 *
 * Embeds the TI DRV2605L, a haptic driver for LRA (Linear Resonant
 * Actuator) and ERM actuators with a built-in waveform library
 * of 123 effects.
 *
 * Key specifications:
 *   - Output:       full-bridge, 3.0–5.2 Vrms into LRA
 *   - Waveform lib: 123 haptic effects (6 libraries)
 *   - Auto-cal:     automatic resonance tracking for LRA
 *   - Modes:        Internal trigger, RTP, PWM, audio-to-vibe
 *
 * Datasheet: https://www.bergsonne.io/tiles/drive/h
 * IC datasheet: https://www.ti.com/lit/ds/symlink/drv2605l.pdf
 *
 * Quick start:
 * @code
 *   #include "core_tiles.h"
 *
 *   tile_t haptic;
 *   tile_drive_h_init(core_tiles_pal(&core_i2c1), 0, &haptic);
 *   if (tile_is_ready(&haptic)) {
 *       tile_drive_h_play(&haptic, 1, 1);  // play effect #1 once
 *   }
 * @endcode
 */

#ifndef INC_TILE_DRIVE_H_H_
#define INC_TILE_DRIVE_H_H_

#include "tiles.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* Driver version                                                  */
/* -------------------------------------------------------------- */

#define TILE_DRIVE_H_VERSION_MAJOR  2
#define TILE_DRIVE_H_VERSION_MINOR  0
#define TILE_DRIVE_H_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);  /* requires tiles.h >= 1.0 */

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

/**
 * @brief  Instance-to-address mapping for Drive.H.
 *
 * | Instance | ID   | Bus  | Hardware config      |
 * |----------|------|------|----------------------|
 * | 0        | 0x5A | I2C  | Fixed address        |
 *
 * @note  The DRV2605L has a single fixed I2C address. Multiple
 *        Drive.H tiles require separate I2C buses.
 */
#define DRV2605L_I2C_ADDR_DEFAULT   0x5A

/* -------------------------------------------------------------- */
/* DRV2605L register map                                           */
/* -------------------------------------------------------------- */

#define DRV2605L_REG_STATUS         0x00  /**< Status register */
#define DRV2605L_REG_MODE           0x01  /**< Mode register */
#define DRV2605L_REG_RTP            0x02  /**< Real-time playback input */
#define DRV2605L_REG_LIBRARY_SEL    0x03  /**< Waveform library selection */
#define DRV2605L_REG_WAVE_SEQ_0     0x04  /**< Waveform sequence slot 0 */
#define DRV2605L_REG_WAVE_SEQ_1     0x05  /**< Waveform sequence slot 1 */
#define DRV2605L_REG_GO             0x0C  /**< Go register (trigger playback) */
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
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_drive_h_find(tiles_pal_t* hal, uint8_t instance);

/**
 * @brief  Initialize the DRV2605L haptic driver.
 *
 * Verifies the status register, exits standby, and configures
 * for LRA open-loop operation with library 6.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @param  tile      Pointer to tile handle (populated by this function)
 *
 * @note   Blocks for ~500 ms during init. Call once at startup.
 */
void tile_drive_h_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile);

/**
 * @brief  Play a waveform effect from the built-in library.
 *
 * Loads the effect index into sequence slot 0, terminates
 * the sequence, and triggers playback. For repeats > 1,
 * re-triggers with a 200ms gap between plays.
 *
 * @param  tile     Pointer to tile handle
 * @param  index    Library effect index (1–123, see datasheet)
 * @param  repeats  Number of times to play (1 = once)
 */
void tile_drive_h_play(tile_t* tile, uint8_t index, uint8_t repeats);

/**
 * @brief  Stop any currently playing effect.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_h_stop(tile_t* tile);

/**
 * @brief  Enter RTP (Real-Time Playback) mode.
 *
 * Sets DRV2605L MODE register to 0x05 (RTP). The chip drives
 * the LRA at its resonant frequency with amplitude controlled
 * by tile_drive_h_rtp_write(). Call tile_drive_h_rtp_stop()
 * to return to internal trigger mode.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_h_rtp_start(tile_t* tile);

/**
 * @brief  Write an amplitude value in RTP mode.
 *
 * @param  tile       Pointer to tile handle
 * @param  amplitude  Unsigned 8-bit amplitude (0 = off, 127 = max)
 */
void tile_drive_h_rtp_write(tile_t* tile, uint8_t amplitude);

/**
 * @brief  Exit RTP mode and return to internal trigger mode.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_h_rtp_stop(tile_t* tile);

#endif /* INC_TILE_DRIVE_H_H_ */
