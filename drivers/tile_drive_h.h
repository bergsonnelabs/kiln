/**
 * @file   tile_drive_h.h
 * @brief  LRA haptic driver for the Drive.H tile (rev a).
 *
 * Embeds the TI DRV2605L, a haptic driver for LRA (Linear Resonant
 * Actuator) and ERM actuators with a built-in waveform library
 * of 123 effects.
 *
 * Key specifications:
 *   - Output:       full-bridge, 3.0-5.2 Vrms into LRA
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
 *   tile_drive_h_init(core_tiles_pal(&core_i2c1), 0, &haptic, NULL);
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

#define TILE_DRIVE_H_VERSION_MAJOR  3
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
#define DRV2605L_REG_WAVE_SEQ_2     0x06  /**< Waveform sequence slot 2 */
#define DRV2605L_REG_WAVE_SEQ_3     0x07  /**< Waveform sequence slot 3 */
#define DRV2605L_REG_WAVE_SEQ_4     0x08  /**< Waveform sequence slot 4 */
#define DRV2605L_REG_WAVE_SEQ_5     0x09  /**< Waveform sequence slot 5 */
#define DRV2605L_REG_WAVE_SEQ_6     0x0A  /**< Waveform sequence slot 6 */
#define DRV2605L_REG_WAVE_SEQ_7     0x0B  /**< Waveform sequence slot 7 */
#define DRV2605L_REG_GO             0x0C  /**< Go register (trigger playback) */
#define DRV2605L_REG_RATED_VOLTAGE  0x16  /**< Rated voltage for actuator */
#define DRV2605L_REG_OD_CLAMP       0x17  /**< Overdrive clamp voltage */
#define DRV2605L_REG_A_CAL_COMP     0x18  /**< Auto-cal compensation result */
#define DRV2605L_REG_A_CAL_BEMF     0x19  /**< Auto-cal back-EMF result */
#define DRV2605L_REG_FEEDBACK_CTRL  0x1A  /**< Feedback control register */
#define DRV2605L_REG_CONTROL1       0x1B  /**< Control1 (startup boost, drive time) */
#define DRV2605L_REG_CONTROL2       0x1C  /**< Control2 (bidir, sample/blank/idiss time) */
#define DRV2605L_REG_CONTROL3       0x1D  /**< Control3 (open loop, RTP format) */
#define DRV2605L_REG_CONTROL4       0x1E  /**< Control4 (auto-cal time, OTP) */
#define DRV2605L_REG_VBAT           0x21  /**< Battery voltage monitor */
#define DRV2605L_REG_LRA_PERIOD     0x22  /**< LRA resonance period */

/** @brief  Expected default STATUS register value (DEVICE_ID = 3). */
#define DRV2605L_STATUS_DEFAULT     0x60

/* -------------------------------------------------------------- */
/* Status register bit masks                                       */
/* -------------------------------------------------------------- */

#define DRV2605L_STATUS_DEVICE_ID   0xE0  /**< Bits 7:5 — device ID */
#define DRV2605L_STATUS_DIAG_RESULT 0x08  /**< Bit 3 — diagnostic result */
#define DRV2605L_STATUS_OVER_TEMP   0x02  /**< Bit 1 — over-temperature */
#define DRV2605L_STATUS_OC_DETECT   0x01  /**< Bit 0 — overcurrent detect */

/* -------------------------------------------------------------- */
/* Mode register values                                            */
/* -------------------------------------------------------------- */

#define DRV2605L_MODE_INTERNAL_TRIG 0x00  /**< Internal trigger (I2C GO) */
#define DRV2605L_MODE_EXT_EDGE      0x01  /**< External trigger, edge mode */
#define DRV2605L_MODE_EXT_LEVEL     0x02  /**< External trigger, level mode */
#define DRV2605L_MODE_RTP           0x05  /**< Real-time playback */
#define DRV2605L_MODE_DIAGNOSTICS   0x06  /**< Actuator diagnostics */
#define DRV2605L_MODE_CALIBRATION   0x07  /**< Auto-calibration */
#define DRV2605L_MODE_STANDBY       0x40  /**< STANDBY bit (bit 6) */

/* -------------------------------------------------------------- */
/* Trigger mode selection                                          */
/* -------------------------------------------------------------- */

/** Internal trigger — waveforms fired via I2C GO bit (default). */
#define DRIVE_H_TRIG_INTERNAL  0
/** Edge trigger — rising edge on IN/TRIG pin fires the sequencer.
 *  A second rising edge while GO is high cancels playback.
 *  Pulse width must be >= 1 µs. */
#define DRIVE_H_TRIG_EDGE      1
/** Level trigger — GO bit follows IN/TRIG pin level.
 *  High = playing, falling edge = cancel. */
#define DRIVE_H_TRIG_LEVEL     2

/* -------------------------------------------------------------- */
/* Sequence limits                                                 */
/* -------------------------------------------------------------- */

/** Maximum effects in a single waveform sequence. */
#define DRV2605L_SEQ_MAX            8

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
 * Optional init config. Pass NULL for defaults (LRA open-loop, library 6,
 * voltage parameters for the Drive.H onboard actuator).
 *
 * To match a different LRA, set rated_voltage and od_clamp using the
 * formulas in the DRV2605 datasheet (section 7.5.2). Typical values:
 *
 * | LRA rated voltage | rated_voltage | od_clamp |
 * |-------------------|---------------|----------|
 * | 0.5 Vrms          | 0x13          | 0x1B     |
 * | 0.7 Vrms          | 0x1A          | 0x25     |
 * | 1.0 Vrms          | 0x26          | 0x36     |
 * | 1.8 Vrms          | 0x56          | 0x8C     |
 */
typedef struct {
    uint8_t library;       /**< Waveform library: 1-5 = ERM (A-E), 6 = LRA.
                                0 = use default (6). */
    uint8_t closed_loop;   /**< 0 = open-loop (default), 1 = closed-loop. */
    uint8_t rated_voltage; /**< RATED_VOLTAGE register (0x16). 0 = default.
                                Depends on actuator rated RMS voltage. */
    uint8_t od_clamp;      /**< OD_CLAMP register (0x17). 0 = default.
                                Overdrive clamp / open-loop ref voltage. */
} drive_h_cfg_t;

/**
 * @brief  Initialize the DRV2605L haptic driver.
 *
 * Verifies the status register, exits standby, and configures the
 * actuator drive mode. Pass cfg=NULL for defaults (LRA open-loop,
 * library 6).
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @param  tile      Pointer to tile handle (populated by this function)
 * @param  cfg       Optional config, or NULL for defaults
 *
 * @note   Blocks for ~500 ms during init. Call once at startup.
 */
void tile_drive_h_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                       const drive_h_cfg_t *cfg);

/**
 * @brief  Play a waveform effect from the built-in library.
 *
 * Loads the effect index into sequence slot 0, terminates
 * the sequence, and triggers playback. For repeats > 1,
 * re-triggers with a 200ms gap between plays.
 *
 * @param  tile     Pointer to tile handle
 * @param  index    Library effect index (1-123, see datasheet)
 * @param  repeats  Number of times to play (1 = once)
 */
void tile_drive_h_play(tile_t* tile, uint8_t index, uint8_t repeats);

/**
 * @brief  Play a sequence of up to 8 waveform effects.
 *
 * Loads effects into the waveform sequencer registers (0x04-0x0B),
 * terminates the sequence, and triggers playback. Returns
 * immediately — use tile_drive_h_is_playing() to poll for
 * completion.
 *
 * @param  tile     Pointer to tile handle
 * @param  effects  Array of effect indices (1-123)
 * @param  count    Number of effects (1-8)
 */
void tile_drive_h_play_sequence(tile_t* tile, const uint8_t *effects,
                                uint8_t count);

/**
 * @brief  Load effects into the waveform sequencer without triggering.
 *
 * Identical to tile_drive_h_play_sequence() but does NOT assert
 * the GO bit. Use this to pre-load effects before switching to
 * external trigger mode (edge or level).
 *
 * @param  tile     Pointer to tile handle
 * @param  effects  Array of effect indices (1-123)
 * @param  count    Number of effects (1-8)
 */
void tile_drive_h_load_sequence(tile_t* tile, const uint8_t *effects,
                                uint8_t count);

/**
 * @brief  Set the trigger mode.
 *
 * Selects how the waveform sequencer is triggered:
 *   - DRIVE_H_TRIG_INTERNAL (default): I2C GO bit, used by play()
 *     and play_sequence().
 *   - DRIVE_H_TRIG_EDGE: a rising edge on the IN/TRIG pin sets
 *     GO. A second rising edge while playing cancels. Pre-load
 *     effects with tile_drive_h_load_sequence() before entering
 *     this mode.
 *   - DRIVE_H_TRIG_LEVEL: GO follows the IN/TRIG pin level.
 *     High = playing, low = idle. Falling edge cancels.
 *
 * @param  tile  Pointer to tile handle
 * @param  mode  One of DRIVE_H_TRIG_INTERNAL, DRIVE_H_TRIG_EDGE,
 *               DRIVE_H_TRIG_LEVEL
 */
void tile_drive_h_set_trigger(tile_t* tile, uint8_t mode);

/**
 * @brief  Check whether an effect or sequence is still playing.
 *
 * Reads the GO bit in register 0x0C. The GO bit remains high
 * until playback completes.
 *
 * @param  tile  Pointer to tile handle
 * @return 1 if playing, 0 if idle
 */
uint8_t tile_drive_h_is_playing(tile_t* tile);

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

/**
 * @brief  Read the raw STATUS register (0x00).
 *
 * Contains DEVICE_ID[7:5], DIAG_RESULT[3], FB_STS[2],
 * OVER_TEMP[1], OC_DETECT[0]. Status bits clear on read.
 *
 * @param  tile  Pointer to tile handle
 * @return Raw status byte
 */
uint8_t tile_drive_h_get_status(tile_t* tile);

/**
 * @brief  Run actuator diagnostics.
 *
 * Enters diagnostic mode (MODE=6), triggers GO, and polls
 * for completion. The DRV2605L checks whether the actuator is
 * present, open, or shorted.
 *
 * @param  tile  Pointer to tile handle
 * @return 1 if actuator passed diagnostics, 0 if fault detected
 */
uint8_t tile_drive_h_diagnose(tile_t* tile);

/**
 * @brief  Run auto-calibration for the connected actuator.
 *
 * Enters calibration mode (MODE=7) with datasheet-recommended
 * parameters, triggers GO, and polls for completion. On success,
 * the DRV2605L stores optimized A_CAL_COMP and A_CAL_BEMF values
 * that improve playback fidelity.
 *
 * @param  tile  Pointer to tile handle
 * @return 1 if calibration passed, 0 if it failed to converge
 */
uint8_t tile_drive_h_calibrate(tile_t* tile);

/**
 * @brief  Read supply voltage in millivolts.
 *
 * Reads the VBAT register (0x21). The reading is only valid
 * while the device is actively driving a waveform (RTP, library
 * playback, etc.).
 *
 * @param  tile  Pointer to tile handle
 * @return Battery voltage in mV (e.g. 3300 = 3.3 V), 0 if idle
 */
uint16_t tile_drive_h_get_vbat_mv(tile_t* tile);

/**
 * @brief  Read LRA resonant frequency in Hz.
 *
 * Reads the LRA_PERIOD register (0x22). The reading is only
 * valid while the device is actively driving a waveform and
 * must not be polled during braking.
 *
 * @param  tile  Pointer to tile handle
 * @return Resonant frequency in Hz (e.g. 235), 0 if unavailable
 */
uint16_t tile_drive_h_get_resonance_hz(tile_t* tile);

/**
 * @brief  Enter low-power standby.
 *
 * Sets the STANDBY bit in the MODE register. The device retains
 * register values and can be woken quickly with
 * tile_drive_h_wake().
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_h_standby(tile_t* tile);

/**
 * @brief  Wake from standby.
 *
 * Clears the STANDBY bit in the MODE register. The device
 * returns to the active state, ready for playback.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_h_wake(tile_t* tile);

#endif /* INC_TILE_DRIVE_H_H_ */
