/**
 * @file   tile_drive_p.h
 * @brief  Piezoelectric haptic driver for the Drive.P tile (rev a).
 *
 * Embeds the Boréas Technologies BOS1921, a piezoelectric driver
 * with integrated high-voltage boost (190 Vpp differential),
 * waveform synthesizer, 1024-sample FIFO, and piezo sensing.
 *
 * Key specifications:
 *   - Output:      190 Vpp differential, up to 820 nF capacitive load
 *   - Sensing:     7.6 mV resolution (fine), 54.5 mV (coarse)
 *   - Play modes:  Direct, FIFO, RAM, RAM Synth
 *   - Sample rate:  8 ksps – 1024 ksps (configurable)
 *
 * Datasheet: https://www.bergsonne.io/tiles/drive/p
 * IC datasheet: https://mosaic-component-datasheets.s3.eu-north-1.amazonaws.com/5/Bor_as_Technologies-BOS1921.pdf
 *
 * @tessera tile label=Drive.P icon=♪
 *
 * Quick start:
 * @code
 *   #include "core_tiles.h"
 *
 *   tile_t piezo;
 *   tile_drive_p_init(core_tiles_pal(&core_i2c1), 0, &piezo, NULL);
 *   if (tile_is_ready(&piezo)) {
 *       tile_drive_p_set_mode(&piezo, DRIVE_P_MODE_PLAY_FIFO);
 *       tile_drive_p_write_fifo(&piezo, 0x7FFF);
 *   }
 * @endcode
 *
 * Driver gaps (chip capabilities not exposed by this driver):
 *
 * @tessera unsupported severity=advanced category="Multi-device SYNC pin"
 *   SYNC pin coordinates phase between cascaded BOS1921s
 *   (< 2 µs delay). The Drive.P tile has 10 pads (I2C/I3C, OUT±,
 *   GPIO, V+, V_DRIVE, GND); the SYNC pin on the IC is not routed
 *   to a pad, so multi-tile cascading is hardware-gated to a
 *   future tile rev.
 *
 * @tessera unsupported severity=advanced category="I3C alternate bus mode"
 *   Pads 4/5 are bus-shared between I²C (default) and I3C SDR
 *   (≤12.5 Mbps with in-band interrupts). The cores tile-driver
 *   framework only ships an I²C PAL; I3C support is an
 *   ecosystem-wide gap, not BOS1921-specific.
 */

#ifndef INC_TILE_DRIVE_P_H_
#define INC_TILE_DRIVE_P_H_

#include "tiles.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* Driver version                                                  */
/* -------------------------------------------------------------- */

#define TILE_DRIVE_P_VERSION_MAJOR  3
#define TILE_DRIVE_P_VERSION_MINOR  0
#define TILE_DRIVE_P_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);  /* requires tiles.h >= 1.0 */

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

/**
 * @brief  Instance-to-address mapping for Drive.P.
 *
 * | Instance | ID   | Bus  | Hardware config      |
 * |----------|------|------|----------------------|
 * | 0        | 0x44 | I2C  | Fixed address        |
 *
 * @note  The BOS1921 has a single fixed I2C address. Multiple
 *        Drive.P tiles require separate I2C buses.
 */
#define BOS1921_I2C_ADDR_DEFAULT    0x44

/* -------------------------------------------------------------- */
/* BOS1921 register map                                            */
/* -------------------------------------------------------------- */

#define BOS1921_REG_REFERENCE       0x00  /**< Waveform reference / FIFO write */
#define BOS1921_REG_CONFIG          0x05  /**< Main configuration register */
#define BOS1921_REG_PARCAP          0x06  /**< Parasitic capacitance trim */
#define BOS1921_REG_SUP_RISE        0x07  /**< Supply rise time */
#define BOS1921_REG_COMM            0x0B  /**< Communication / return register select */
#define BOS1921_REG_IC_STATUS       0x10  /**< IC status register */
#define BOS1921_REG_SENSE_VAL       0x18  /**< Sensed piezo voltage */
#define BOS1921_REG_CHIP_ID         0x1E  /**< Chip identification */

/** @brief  Expected lower 12 bits of CHIP_ID register. */
#define BOS1921_CHIP_ID_DEFAULT     0x0781

/* -------------------------------------------------------------- */
/* Status masks                                                    */
/* -------------------------------------------------------------- */

/** @brief  STATE field in IC_STATUS (bits 9:8). */
#define BOS_STATUS_STATE_MASK       0x0300
#define BOS_STATUS_STATE_IDLE       0x0000
#define BOS_STATUS_STATE_CALIB      0x0100
#define BOS_STATUS_STATE_RUNNING    0x0200
#define BOS_STATUS_STATE_ERROR      0x0300

/** @brief  Fault bits in IC_STATUS (bits 7:2, excluding FULL and PLAYST). */
#define BOS_STATUS_FAULT_MASK       0x00FC

/** @brief  CONFIG register bit fields (see datasheet §6.10.6). */
#define BOS_CONFIG_GAINS_BIT        (1u << 12)  /**< 0=54.5 mV LSB, 1=7.6 mV LSB (default 1) */
#define BOS_CONFIG_GAIND_BIT        (1u << 11)  /**< 0=±95 V output, 1=±13.28 V output (default 0) */
#define BOS_CONFIG_RET_BIT          (1u << 8)   /**< 1=clear RAM/regs in SLEEP, 0=retain (default 0) */

/** @brief  COMM register bit fields (see datasheet §6.10.12). */
#define BOS_COMM_TOUT_BIT           (1u << 5)   /**< 1=auto-sleep after 4 ms idle in Direct/FIFO */

/** @brief  PARCAP register bit fields (see datasheet §6.10.7). */
#define BOS_PARCAP_UPI_BIT          (1u << 9)   /**< 1=Unidirectional Power Input (sink-only) */

/* -------------------------------------------------------------- */
/* Operating modes                                                 */
/* -------------------------------------------------------------- */

/**
 * @brief  Drive.P operating mode.
 *
 * Each mode configures the CONFIG register for a specific use case.
 * After init, the device is in IDLE mode.
 */
typedef enum {
    DRIVE_P_MODE_IDLE           = 0,  /**< Output disabled, status readback */
    DRIVE_P_MODE_SENSE_FINE     = 1,  /**< Piezo sensing, 7.6 mV/LSB resolution */
    DRIVE_P_MODE_SENSE_COARSE   = 2,  /**< Piezo sensing, 54.5 mV/LSB resolution */
    DRIVE_P_MODE_PLAY_DIRECT    = 3,  /**< Direct waveform output */
    DRIVE_P_MODE_PLAY_FIFO      = 4,  /**< FIFO-buffered playback, 8 ksps */
    DRIVE_P_MODE_PLAY_RAM_SYNTH = 5,  /**< RAM Synthesis waveform playback */
} drive_p_mode_t;

/**
 * @brief  Output voltage range (CONFIG.GAIND).
 *
 * @note   Switching range invalidates the PARCAP / TI_RISE values
 *         set during init (those are computed against the configured
 *         FBratio, which is determined by GAIND). After changing the
 *         range, recompute and rewrite PARCAP and TI_RISE for the
 *         new FBratio if precise output behaviour matters.
 */
typedef enum {
    DRIVE_P_OUTPUT_HIGH_V = 0,  /**< ±95 V range, FBratio 31 (default) */
    DRIVE_P_OUTPUT_LOW_V  = 1,  /**< ±13.28 V range, FBratio 4.33 */
} drive_p_output_range_t;

/**
 * @brief  Sense-channel resolution (CONFIG.GAINS).
 */
typedef enum {
    DRIVE_P_SENSE_COARSE_GAIN = 0,  /**< 54.5 mV LSB, FBratio 31 */
    DRIVE_P_SENSE_FINE_GAIN   = 1,  /**< 7.6 mV LSB, FBratio 4.33 (default) */
} drive_p_sense_gain_t;

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

/**
 * @brief  Check whether a BOS1921 is present on the I2C bus.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_drive_p_find(tiles_pal_t* hal, uint8_t instance);

/**
 * Optional init config. Pass NULL for defaults.
 * Reserved for future use.
 */
typedef struct {
    uint8_t reserved;   /**< Placeholder — no options yet. */
} drive_p_cfg_t;

/**
 * @brief  Initialize the BOS1921 piezoelectric driver.
 *
 * Wakes the device, performs a software reset, verifies the chip ID,
 * and configures parasitic capacitance and supply parameters for a
 * 260nF piezo on a 3.7V LiPo supply. Pass cfg=NULL for defaults.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @param  tile      Pointer to tile handle (populated by this function)
 * @param  cfg       Optional config, or NULL for defaults
 */
void tile_drive_p_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                       const drive_p_cfg_t *cfg);

/**
 * @brief  Perform a software reset.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_p_reset(tile_t* tile);

/**
 * @brief  Set the operating mode.
 * @tessera expose category=tile name=set_mode
 *
 * @param  tile  Pointer to tile handle
 * @param  mode  One of the drive_p_mode_t values
 */
void tile_drive_p_set_mode(tile_t* tile, drive_p_mode_t mode);

/**
 * @brief  Read the current return register value.
 * @tessera expose category=tile name=read returns=int
 *
 * @param  tile  Pointer to tile handle
 * @return 16-bit value from the currently selected return register
 */
uint16_t tile_drive_p_read(tile_t* tile);

/**
 * @brief  Read the sensed piezo voltage.
 * @tessera expose category=tile name=read_sense returns=int
 *
 * Must be in SENSE_FINE or SENSE_COARSE mode.
 *
 * @param  tile  Pointer to tile handle
 * @return Signed 16-bit sense value (−2048 to +2047)
 */
int16_t tile_drive_p_read_sense(tile_t* tile);

/**
 * @brief  Read the IC status register.
 * @tessera expose category=tile name=read_status returns=int
 *
 * @param  tile  Pointer to tile handle
 * @return 16-bit IC_STATUS value
 */
uint16_t tile_drive_p_read_status(tile_t* tile);

/**
 * @brief  Write a sample to the FIFO.
 * @tessera expose category=tile name=write_fifo
 *
 * @param  tile    Pointer to tile handle
 * @param  sample  Signed 16-bit waveform sample
 */
void tile_drive_p_write_fifo(tile_t* tile, int16_t sample);

/**
 * @brief  Write a raw 16-bit value to any BOS1921 register.
 *
 * @param  tile   Pointer to tile handle
 * @param  reg    8-bit register address
 * @param  value  16-bit value (sent big-endian on the wire)
 */
void tile_drive_p_write_reg(tile_t* tile, uint8_t reg, uint16_t value);

/**
 * @brief  Write a multi-word WFS command to the BOS1921.
 *
 * @param  tile   Pointer to tile handle
 * @param  words  Array of 16-bit words (big-endian on wire)
 * @param  count  Number of words (max 8)
 */
void tile_drive_p_wfs_write(tile_t* tile, const uint16_t* words, uint16_t count);

/**
 * @brief  Enter low-power sleep mode.
 * @tessera expose category=tile name=sleep
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_p_sleep(tile_t* tile);

/**
 * @brief  Check status and recover from error/fault states.
 * @tessera expose category=tile name=check_and_recover returns=bool
 *
 * @param  tile          Pointer to tile handle
 * @param  restore_mode  Mode to re-enter after recovery
 * @return 1 if recovery was performed, 0 if device was healthy
 */
uint8_t tile_drive_p_check_and_recover(tile_t* tile, drive_p_mode_t restore_mode);

/**
 * @brief  Select the output voltage range (CONFIG.GAIND).
 * @tessera expose category=tile name=set_output_range
 *
 * High-V (±95 V) is the BOS1921 default and suits most piezo
 * actuators. Low-V (±13.28 V) is for low-voltage piezos where the
 * full ±95 V swing would be wasteful or destructive. Use this from
 * IDLE before setting a play mode — the change takes effect on the
 * next OE-enable.
 *
 * @note  Changing range invalidates the PARCAP / TI_RISE tuning set
 *        at init. If accurate output behaviour matters, recompute
 *        and rewrite those registers for the new FBratio (see
 *        datasheet §7.5).
 *
 * @param  tile   Pointer to tile handle
 * @param  range  DRIVE_P_OUTPUT_HIGH_V or DRIVE_P_OUTPUT_LOW_V
 */
void tile_drive_p_set_output_range(tile_t* tile, drive_p_output_range_t range);

/**
 * @brief  Select the sense-channel resolution (CONFIG.GAINS).
 * @tessera expose category=tile name=set_sense_gain
 *
 * Fine gain (7.6 mV LSB) is the BOS1921 default and gives the
 * highest sensing resolution. Coarse gain (54.5 mV LSB) widens the
 * input range — useful when sensing high-amplitude press events
 * that would otherwise saturate at fine gain. Use from IDLE before
 * entering a sense mode.
 *
 * @param  tile  Pointer to tile handle
 * @param  gain  DRIVE_P_SENSE_FINE_GAIN or DRIVE_P_SENSE_COARSE_GAIN
 */
void tile_drive_p_set_sense_gain(tile_t* tile, drive_p_sense_gain_t gain);

/**
 * @brief  Configure register and RAM retention during SLEEP (CONFIG.RET).
 * @tessera expose category=tile name=set_sleep_retention
 *
 * Default is retain (~2.4 µA quiescent) so that RAM contents and
 * register configuration survive a sleep cycle. Disabling retention
 * (~0.6 µA) is useful for ultra-low-power applications that re-init
 * on every wake anyway. Set this before calling sleep().
 *
 * @param  tile    Pointer to tile handle
 * @param  retain  1 = retain (default), 0 = clear on sleep
 */
void tile_drive_p_set_sleep_retention(tile_t* tile, uint8_t retain);

/**
 * @brief  Enable or disable the auto-sleep timeout (COMM.TOUT).
 * @tessera expose category=tile name=set_auto_sleep
 *
 * When enabled, the device drops into SLEEP after 4 ms of bus
 * inactivity during Direct or FIFO playback. Useful for unattended
 * one-shot waveforms; harmful for long streaming playback where
 * a host gap would unexpectedly stop the output.
 *
 * @note  After a timeout-triggered sleep, PLAY_SRATE is reset to
 *        0x7 (8 ksps) — re-set the sample rate before the next
 *        playback if you were using a faster rate.
 *
 * @param  tile     Pointer to tile handle
 * @param  enabled  1 = auto-sleep on idle, 0 = stay awake
 */
void tile_drive_p_set_auto_sleep(tile_t* tile, uint8_t enabled);

/**
 * @brief  Enable or disable the Unidirectional Power Input (PARCAP.UPI).
 * @tessera expose category=tile name=set_upi
 *
 * UPI forces the BOS1921 into sink-only operation: energy
 * recovered from piezo discharge is dumped instead of pushed back
 * into the supply. Useful for battery-powered designs where the
 * supply rail can't safely absorb returned energy.
 *
 * @param  tile     Pointer to tile handle
 * @param  enabled  1 = sink-only (UPI on), 0 = energy recovery (default)
 */
void tile_drive_p_set_upi(tile_t* tile, uint8_t enabled);

#endif /* INC_TILE_DRIVE_P_H_ */
