/**
 * @file   tile_sense_mic.h
 * @brief  Complete driver for the Sense.MIC tile (MAX11645 ADC + AMM-2742 MEMS mic).
 *         I2C-only, command-based protocol via tiles_pal_t raw I2C.
 * @version 1.0.0
 *
 * I2C-output MEMS microphone tile combining:
 *   - PUI Audio AMM-2742-T-R: omnidirectional MEMS mic, 20 Hz–20 kHz,
 *     59 dB SNR, 123 dB AOP
 *   - Maxim MAX11645: 12-bit 2-channel ADC, up to 94.4 ksps,
 *     I2C up to 1.7 MHz, internal 2.048V reference
 *
 * The MAX11645 uses a command-based I2C protocol (no register addresses).
 * All bus access goes through tiles_pal_t i2c_write_raw / i2c_read_raw.
 *
 * Platform-agnostic: uses tiles_pal_t for all bus access.
 *
 * Quick start — polling:
 * @code
 *   tile_t mic;
 *   tile_sense_mic_init(core_tiles_pal(&core_i2c3), 0, &mic, NULL);
 *   uint16_t raw = tile_sense_mic_get_raw(&mic);
 *   uint16_t mv  = tile_sense_mic_get_raw_mv(&mic);
 * @endcode
 *
 * Quick start — continuous sampling:
 * @code
 *   tile_t mic;
 *   tile_sense_mic_init(core_tiles_pal(&core_i2c3), 0, &mic, NULL);
 *
 *   uint16_t samples[256];
 *   tile_sense_mic_get_samples(&mic, samples, 256);
 *
 *   uint16_t dc  = tile_sense_mic_dc_level(samples, 256);
 *   uint16_t pp  = tile_sense_mic_peak_to_peak(samples, 256);
 * @endcode
 *
 * Quick start — with internal reference for precise mV:
 * @code
 *   sense_mic_cfg_t cfg = { .ref = SENSE_MIC_REF_INTERNAL };
 *   tile_t mic;
 *   tile_sense_mic_init(core_tiles_pal(&core_i2c3), 0, &mic, &cfg);
 *   // Now get_raw_mv() uses 2048 mV full-scale (0.5 mV/LSB)
 * @endcode
 *
 * Datasheet: Maxim MAX11644/MAX11645, 19-4544; Rev 3, 9/09
 */

#ifndef INC_TILE_SENSE_MIC_H_
#define INC_TILE_SENSE_MIC_H_

#include "tiles.h"
#include <stdint.h>

/* ================================================================
 * Driver version
 * ================================================================ */

#define TILE_SENSE_MIC_VERSION_MAJOR  1
#define TILE_SENSE_MIC_VERSION_MINOR  0
#define TILE_SENSE_MIC_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);

/* ================================================================
 * Instance mapping
 * ================================================================ */

/**
 * | Instance | ID   | Hardware config                       |
 * |----------|------|---------------------------------------|
 * | 0        | 0x36 | Fixed address — no address pin        |
 *
 * The MAX11645 has a factory-set I2C address of 0x36. Only one
 * instance is supported per bus.
 */
#define MAX11645_I2C_ADDR  0x36

/* ================================================================
 * MAX11645 protocol constants
 *
 * The MAX11645 uses a command-based I2C protocol. Two command
 * byte types are distinguished by bit 7:
 *   - Setup byte   (bit 7 = 1): configures Vref, clock, polarity
 *   - Config byte  (bit 7 = 0): configures scan mode, channel, mode
 *
 * Conversion data is returned as 2 bytes (big-endian, 12-bit
 * right-aligned) via a raw I2C read with no preceding address.
 * ================================================================ */

/* --- Setup byte (bit 7 = 1) --- */

#define MAX11645_SETUP_REG       (1 << 7)   /**< Bit 7 = 1 → setup byte */

/* SEL[2:0]: bits 6:4 — reference voltage selection */
#define MAX11645_SEL_VDD         (0x00 << 4)  /**< VDD as reference (default) */
#define MAX11645_SEL_EXT         (0x01 << 4)  /**< External ref on REF pin */
#define MAX11645_SEL_INT_ON      (0x02 << 4)  /**< Internal 2.048V, always on */
#define MAX11645_SEL_INT_ON_BUF  (0x03 << 4)  /**< Internal 2.048V, buffered output */
#define MAX11645_SEL_EXT2        (0x04 << 4)  /**< External ref (same as 001) */
#define MAX11645_SEL_EXT_BUF     (0x05 << 4)  /**< External ref, buffered */
#define MAX11645_SEL_AIN1_OFF    (0x06 << 4)  /**< AIN1/REF input, ref off after setup */
#define MAX11645_SEL_AIN1_ON     (0x07 << 4)  /**< AIN1/REF input, ref on */

/* CLK: bit 3 */
#define MAX11645_CLK_INTERNAL    (0 << 3)     /**< Internal clock (default) */
#define MAX11645_CLK_EXTERNAL    (1 << 3)     /**< External clock on AIN1 */

/* BIP/UNI: bit 2 */
#define MAX11645_UNI             (0 << 2)     /**< Unipolar output (default) */
#define MAX11645_BIP             (1 << 2)     /**< Bipolar output (two's comp) */

/* RST: bit 1 */
#define MAX11645_RST_NORESET     (1 << 1)     /**< No action on config register */
#define MAX11645_RST_RESET       (0 << 1)     /**< Reset config register to default */

/* --- Configuration byte (bit 7 = 0) --- */

#define MAX11645_CONFIG_REG      (0 << 7)     /**< Bit 7 = 0 → config byte */

/* SCAN[1:0]: bits 6:5 — scan mode */
#define MAX11645_SCAN_UP         (0x00 << 5)  /**< Scan from AIN0 up to CS0 channel */
#define MAX11645_SCAN_8X         (0x01 << 5)  /**< Convert CS0 channel 8 times */
#define MAX11645_SCAN_UPPER      (0x02 << 5)  /**< Scan from CS0 up to highest channel */
#define MAX11645_SCAN_SINGLE     (0x03 << 5)  /**< Convert CS0 channel only */

/* CS0: bit 1 — channel select (2-channel MAX11645) */
#define MAX11645_CS0_AIN0        (0 << 1)     /**< Select AIN0 (mic input) */
#define MAX11645_CS0_AIN1        (1 << 1)     /**< Select AIN1 */

/* SGL/DIF: bit 0 */
#define MAX11645_SINGLE_ENDED    (1 << 0)     /**< Single-ended (default) */
#define MAX11645_DIFFERENTIAL    (0 << 0)     /**< Differential */

/* --- ADC resolution --- */

#define MAX11645_ADC_BITS        12
#define MAX11645_ADC_MAX         ((1 << MAX11645_ADC_BITS) - 1)  /**< 4095 */

/* ================================================================
 * Configuration enums
 * ================================================================ */

/**
 * @brief  Reference voltage selection.
 *
 * | Enum value        | Vref     | Notes                              |
 * |-------------------|----------|------------------------------------|
 * | REF_VDD           | VDD      | Simple, full-range (default)       |
 * | REF_INTERNAL      | 2.048V   | Precise, always on                 |
 * | REF_INTERNAL_BUF  | 2.048V   | Buffered output on REF pin         |
 * | REF_EXTERNAL      | REF pin  | User-supplied reference             |
 * | REF_EXTERNAL_BUF  | REF pin  | User-supplied, buffered             |
 */
typedef enum {
    SENSE_MIC_REF_VDD          = 0x00,  /**< VDD reference (default) */
    SENSE_MIC_REF_EXTERNAL     = 0x01,  /**< External ref on REF pin */
    SENSE_MIC_REF_INTERNAL     = 0x02,  /**< Internal 2.048V, always on */
    SENSE_MIC_REF_INTERNAL_BUF = 0x03,  /**< Internal 2.048V, buffered */
    SENSE_MIC_REF_EXTERNAL_BUF = 0x05,  /**< External ref, buffered */
} sense_mic_ref_t;

/**
 * @brief  ADC channel selection.
 *
 * | Enum value | Channel | Tile signal                         |
 * |------------|---------|-------------------------------------|
 * | CH_AIN0    | AIN0    | MEMS microphone output (default)    |
 * | CH_AIN1    | AIN1    | AIN1 / reference input              |
 */
typedef enum {
    SENSE_MIC_CH_AIN0 = 0,  /**< AIN0 — microphone (default) */
    SENSE_MIC_CH_AIN1 = 1,  /**< AIN1 */
} sense_mic_channel_t;

/**
 * @brief  Scan mode.
 *
 * | Enum value   | Behavior                                 |
 * |--------------|------------------------------------------|
 * | SCAN_UP      | Scan from AIN0 up to selected channel (default) |
 * | SCAN_SINGLE  | Convert selected channel once            |
 * | SCAN_8X      | Convert selected channel 8 times         |
 */
typedef enum {
    SENSE_MIC_SCAN_SINGLE = 0x03,  /**< Single channel conversion (default) */
    SENSE_MIC_SCAN_UP     = 0x00,  /**< Scan from AIN0 up to CS0 */
    SENSE_MIC_SCAN_8X     = 0x01,  /**< Convert CS0 8 times (averaging) */
} sense_mic_scan_t;

/* ================================================================
 * Configuration
 * ================================================================ */

/**
 * Optional init config. Pass NULL for defaults (VDD ref, AIN0, single-ended).
 */
typedef struct {
    uint8_t  ref;       /**< sense_mic_ref_t. 0 = default (VDD). */
    uint8_t  channel;   /**< sense_mic_channel_t. 0 = default (AIN0 / mic). */
    uint8_t  scan;      /**< sense_mic_scan_t. 0 = default (scan up). */
    uint16_t vref_mv;   /**< Reference voltage in mV. 0 = auto (3300 for VDD,
                             2048 for internal). Only needed for external ref. */
} sense_mic_cfg_t;

/* ================================================================
 * Public API — Lifecycle
 * ================================================================ */

/**
 * @brief  Check if a Sense.MIC is present on the bus (address probe).
 *
 * Lightweight probe — does not configure the device.
 * The MAX11645 has no WHO_AM_I register; this checks for an ACK at 0x36.
 */
uint8_t tile_sense_mic_find(tiles_pal_t *hal, uint8_t instance);

/**
 * @brief  Initialize the MAX11645 ADC.
 *
 * Sends setup byte (reference, clock, polarity) and configuration byte
 * (scan mode, channel, single-ended). Verifies the device responds by
 * performing a test read.
 *
 * Pass cfg=NULL for defaults: VDD reference, AIN0 (mic), single-ended,
 * unipolar, internal clock, single-channel scan.
 *
 * @note   Blocks for ~5 ms (setup settling + DC offset calibration).
 *         Call once at startup.
 */
void tile_sense_mic_init(tiles_pal_t *hal, uint8_t instance,
                         tile_t *tile, const sense_mic_cfg_t *cfg);

/** @brief  Enter low-power mode (no conversions). */
void tile_sense_mic_sleep(tile_t *tile);

/** @brief  Wake from sleep, restore previous configuration. */
void tile_sense_mic_wake(tile_t *tile);

/** @brief  Reset the config register to power-on defaults. Must call init() again. */
void tile_sense_mic_reset(tile_t *tile);

/* ================================================================
 * Public API — Configuration
 * ================================================================ */

/**
 * @brief  Change the reference voltage source.
 *
 * @note   When switching to internal ref, allow ~10 µs for settling.
 *         This function inserts a 1 ms delay automatically.
 */
void tile_sense_mic_set_reference(tile_t *tile, sense_mic_ref_t ref);

/**
 * @brief  Change the active ADC channel.
 *
 * @param  ch  SENSE_MIC_CH_AIN0 (mic) or SENSE_MIC_CH_AIN1
 */
void tile_sense_mic_set_channel(tile_t *tile, sense_mic_channel_t ch);

/**
 * @brief  Change the scan mode.
 *
 * @param  scan  SENSE_MIC_SCAN_SINGLE, SENSE_MIC_SCAN_UP, or SENSE_MIC_SCAN_8X
 */
void tile_sense_mic_set_scan_mode(tile_t *tile, sense_mic_scan_t scan);

/**
 * @brief  Get the currently configured reference voltage in millivolts.
 *
 * Returns the Vref used for millivolt conversions (3300 for VDD, 2048
 * for internal, or the user-specified value for external).
 */
uint16_t tile_sense_mic_get_vref_mv(tile_t *tile);

/* ================================================================
 * Public API — Data reads
 * ================================================================ */

/**
 * @brief  Read a single 12-bit ADC sample (0–4095).
 *
 * The MAX11645 auto-converts on every read — no trigger needed.
 * Conversion time is ~3.5 µs (internal clock); the I2C transaction
 * itself dominates the timing (~55 µs at 400 kHz).
 */
uint16_t tile_sense_mic_get_raw(tile_t *tile);

/**
 * @brief  Read a single sample and convert to millivolts.
 *
 * Conversion: mv = raw * vref_mv / 4096
 */
uint16_t tile_sense_mic_get_raw_mv(tile_t *tile);

/**
 * @brief  Read a single AC-coupled audio sample (signed, relative to DC offset).
 *
 * Returns: (raw - dc_offset) where dc_offset is auto-calibrated during init().
 * Useful for audio waveform capture where DC bias is removed.
 *
 * @return Signed 16-bit value. 0 = silence.
 */
int16_t tile_sense_mic_get_audio_sample(tile_t *tile);

/**
 * @brief  Get the auto-calibrated DC offset (mic bias point).
 *
 * Measured during init() by averaging 64 samples. Typically 600–900
 * with VDD reference, depending on supply voltage and PCB layout.
 */
uint16_t tile_sense_mic_get_dc_offset(tile_t *tile);

/**
 * @brief  Re-calibrate the DC offset (e.g., after changing reference).
 *
 * Averages 64 samples to update the stored bias point.
 * @note   Call in a quiet environment for best results.
 */
void tile_sense_mic_calibrate(tile_t *tile);

/**
 * @brief  Burst-read N samples into a buffer.
 *
 * Reads are back-to-back; effective sample rate depends on I2C bus speed:
 *   - 400 kHz: ~12.5 ksps
 *   - 1 MHz:   ~16 ksps
 *
 * @param  buf    Output buffer (caller-allocated, min count entries)
 * @param  count  Number of samples to read
 */
void tile_sense_mic_get_samples(tile_t *tile, uint16_t *buf, uint16_t count);

/* ================================================================
 * Public API — Audio analysis utilities
 *
 * These operate on sample buffers and are pure computation — no I2C.
 * Pass buffers obtained from tile_sense_mic_get_samples().
 * ================================================================ */

/**
 * @brief  Compute the DC level (mean) of a sample buffer.
 *
 * Useful for determining the mic bias point. Varies with supply
 * voltage and PCB bias circuit (typically 600–900 with VDD ref).
 */
uint16_t tile_sense_mic_dc_level(const uint16_t *samples, uint16_t count);

/**
 * @brief  Compute peak-to-peak amplitude of a sample buffer (raw counts).
 *
 * Returns max - min across all samples. Silence ≈ 5–20 counts (noise floor).
 */
uint16_t tile_sense_mic_peak_to_peak(const uint16_t *samples, uint16_t count);

/**
 * @brief  Compute RMS amplitude relative to a DC offset (raw counts).
 *
 * @param  dc_offset  DC bias point (use dc_level() to measure, or pass 2048)
 * @return RMS of AC component in raw ADC counts
 */
uint16_t tile_sense_mic_rms(const uint16_t *samples, uint16_t count,
                            uint16_t dc_offset);

/**
 * @brief  Convert peak-to-peak raw amplitude to millivolts.
 *
 * Uses the configured reference voltage to scale a peak-to-peak raw
 * ADC count into millivolts: mv = pp_raw * vref_mv / 4096.
 *
 * @param  tile    Initialised tile handle.
 * @param  pp_raw  Peak-to-peak amplitude in raw ADC counts (from peak_to_peak()).
 * @return Amplitude in millivolts.
 */
uint16_t tile_sense_mic_amplitude_mv(tile_t *tile, uint16_t pp_raw);

#endif /* INC_TILE_SENSE_MIC_H_ */
