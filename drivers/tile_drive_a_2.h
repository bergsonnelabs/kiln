/**
 * @file   tile_drive_a_2.h
 * @brief  Dual-channel audio output driver for the Drive.A.2 tile
 *         (DAC63202W smart DAC + 2x TPA2028D1 Class-D amplifiers).
 *         Supports I2C and SPI bus access via tiles_pal_t.
 * @version 2.0.0
 *
 * The Drive.A.2 tile provides two independent audio output channels,
 * each consisting of a 12-bit DAC channel feeding a 3W Class-D amplifier.
 *
 * DAC features (DAC63202W):
 *   - 12-bit resolution, dual channel
 *   - Voltage output with configurable gain (1x, 1.5x, 2x, 3x, 4x)
 *   - Internal 1.21V reference or VDD/external reference
 *   - Built-in waveform generation: sine, triangle, sawtooth
 *   - I2C (up to 1 MHz) or SPI (up to 50 MHz), auto-detected at boot
 *
 * Amplifier features (TPA2028D1, x2):
 *   - 3W into 4 ohm, 880 mW into 8 ohm per channel
 *   - I2C-programmable gain: -28 dB to +30 dB in 1 dB steps
 *   - AGC with configurable compression, attack/release/hold times
 *   - Both amplifiers share I2C address 0x58 — writes affect both
 *
 * In SPI mode, only the DAC is controllable; amplifier functions
 * become no-ops since the I2C bus pins are repurposed for SPI.
 *
 * Quick start (I2C — DAC + amplifiers):
 * @code
 *   #include "core_tiles.h"  // provides core_tiles_pal()
 *
 *   tile_t dac;
 *   tiles_pal_t *hal = core_tiles_pal(&core_i2c1);
 *   tile_drive_a_2_init(hal, 0, &dac, NULL);
 *   if (tile_is_ready(&dac)) {
 *       tile_drive_a_2_set(&dac, 0, 2048);       // Ch 0 mid-scale
 *       tile_drive_a_2_amp_set_gain(&dac, 12);    // +12 dB both amps
 *   }
 * @endcode
 *
 * Quick start (SPI — DAC only, no amplifier control):
 * @code
 *   #include "core_tiles.h"
 *
 *   tile_t dac;
 *   tiles_pal_t *hal = core_tiles_pal(&core_spi1);
 *   tile_drive_a_2_init(hal, 0, &dac, NULL);  // instance = CS index
 *   tile_drive_a_2_set_mv(&dac, 0, 1500);     // 1.5 V on channel 0
 * @endcode
 *
 * Datasheet (DAC): https://www.ti.com/product/DAC63202W
 * Datasheet (Amp): https://www.ti.com/product/TPA2028D1
 */

#ifndef INC_TILE_DRIVE_A_2_H_
#define INC_TILE_DRIVE_A_2_H_

#include "tiles.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* Driver version                                                  */
/* -------------------------------------------------------------- */

#define TILE_DRIVE_A_2_VERSION_MAJOR  2
#define TILE_DRIVE_A_2_VERSION_MINOR  0
#define TILE_DRIVE_A_2_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

/**
 * @brief  Instance-to-address mapping for Drive.A.2.
 *
 * | Instance | DAC addr | A0 pin config             |
 * |----------|----------|---------------------------|
 * | 0        | 0x49     | Float / VDD (default)     |
 * | 1        | 0x48     | GND                       |
 * | 2        | 0x4A     | SDA                       |
 * | 3        | 0x4B     | SCL                       |
 *
 * For SPI, instance selects the CS index (0–7).
 *
 * @note  Tile pads 2 and 3 have weak pull-ups that set I2C mode
 *        and the default address (0x49) at power-on.
 */
#define DAC63202W_I2C_ADDR_DEFAULT   0x49  /**< A0 → VDD (default) */
#define DAC63202W_I2C_ADDR_GND       0x48  /**< A0 → GND */
#define DAC63202W_I2C_ADDR_SDA       0x4A  /**< A0 → SDA */
#define DAC63202W_I2C_ADDR_SCL       0x4B  /**< A0 → SCL */

#define TPA2028D1_I2C_ADDR           0x58  /**< Fixed (both amps) */

/* -------------------------------------------------------------- */
/* DAC63202W register map                                          */
/* -------------------------------------------------------------- */

/* Per-channel registers (X = 0 uses 0x13–0x1C, X = 1 uses 0x01–0x06/0x19) */
#define DAC63202W_REG_DAC_0_MARGIN_HIGH   0x13
#define DAC63202W_REG_DAC_0_MARGIN_LOW    0x14
#define DAC63202W_REG_DAC_0_VOUT_CMP_CFG  0x15
#define DAC63202W_REG_DAC_0_IOUT_MISC_CFG 0x16
#define DAC63202W_REG_DAC_0_CMP_MODE_CFG  0x17
#define DAC63202W_REG_DAC_0_FUNC_CFG      0x18
#define DAC63202W_REG_DAC_0_DATA          0x1C

#define DAC63202W_REG_DAC_1_MARGIN_HIGH   0x01
#define DAC63202W_REG_DAC_1_MARGIN_LOW    0x02
#define DAC63202W_REG_DAC_1_VOUT_CMP_CFG  0x03
#define DAC63202W_REG_DAC_1_IOUT_MISC_CFG 0x04
#define DAC63202W_REG_DAC_1_CMP_MODE_CFG  0x05
#define DAC63202W_REG_DAC_1_FUNC_CFG      0x06
#define DAC63202W_REG_DAC_1_DATA          0x19

/* Shared registers */
#define DAC63202W_REG_COMMON_CONFIG       0x1F
#define DAC63202W_REG_COMMON_TRIGGER      0x20
#define DAC63202W_REG_COMMON_DAC_TRIG     0x21
#define DAC63202W_REG_GENERAL_STATUS      0x22

/** @brief  Expected DEVICE-ID value in GENERAL-STATUS bits[7:2]. */
#define DAC63202W_DEVICE_ID               0x06

/** @brief  Internal reference voltage in mV. */
#define DAC63202W_VREF_INT_MV             1210

/** @brief  DAC resolution in bits. */
#define DAC63202W_DAC_BITS                12
#define DAC63202W_DAC_MAX                 ((1 << DAC63202W_DAC_BITS) - 1)

/* -------------------------------------------------------------- */
/* TPA2028D1 register map                                          */
/* -------------------------------------------------------------- */

#define TPA2028D1_REG_FUNC_CTRL           0x01  /**< EN, SWS, FAULT, Thermal, NG_EN */
#define TPA2028D1_REG_AGC_ATTACK          0x02  /**< Attack time [5:0] */
#define TPA2028D1_REG_AGC_RELEASE         0x03  /**< Release time [5:0] */
#define TPA2028D1_REG_AGC_HOLD            0x04  /**< Hold time [5:0] */
#define TPA2028D1_REG_AGC_GAIN            0x05  /**< Fixed gain [5:0], two's complement */
#define TPA2028D1_REG_AGC_CTRL1           0x06  /**< Limiter disable, noise gate, limiter level */
#define TPA2028D1_REG_AGC_CTRL2           0x07  /**< Max gain [7:4], compression ratio [1:0] */

/* -------------------------------------------------------------- */
/* DAC gain / reference selection                                  */
/* -------------------------------------------------------------- */

/**
 * @brief  DAC output voltage gain setting.
 *
 * Maps directly to VOUT-GAIN-X bits[12:10] in DAC-X-VOUT-CMP-CONFIG.
 * Internal reference gains require EN-INT-REF = 1 in COMMON-CONFIG.
 */
typedef enum {
    DRIVE_A_2_GAIN_1X_EXT   = 0,  /**< 1x, external VREF pin */
    DRIVE_A_2_GAIN_1X_VDD   = 1,  /**< 1x, VDD as reference (default) */
    DRIVE_A_2_GAIN_1P5X_INT = 2,  /**< 1.5x, internal 1.21V → 0–1.815V */
    DRIVE_A_2_GAIN_2X_INT   = 3,  /**< 2x, internal 1.21V → 0–2.42V */
    DRIVE_A_2_GAIN_3X_INT   = 4,  /**< 3x, internal 1.21V → 0–3.63V */
    DRIVE_A_2_GAIN_4X_INT   = 5,  /**< 4x, internal 1.21V → 0–4.84V */
} drive_a_2_gain_t;

/* -------------------------------------------------------------- */
/* Waveform generation                                             */
/* -------------------------------------------------------------- */

/**
 * @brief  Built-in waveform shapes for the DAC function generator.
 *
 * Maps to FUNC-CONFIG-X bits[10:8] in DAC-X-FUNC-CONFIG.
 * Use with set_waveform() / start_waveform() / stop_waveform().
 */
typedef enum {
    DRIVE_A_2_WAVE_TRIANGLE = 0,  /**< Triangular wave */
    DRIVE_A_2_WAVE_SAWTOOTH = 1,  /**< Sawtooth (ramp up) */
    DRIVE_A_2_WAVE_INV_SAW  = 2,  /**< Inverse sawtooth (ramp down) */
    DRIVE_A_2_WAVE_SINE     = 4,  /**< Sine wave */
    DRIVE_A_2_WAVE_OFF      = 7,  /**< Function generation disabled */
} drive_a_2_wave_t;

/* -------------------------------------------------------------- */
/* Amplifier AGC / DRC                                             */
/* -------------------------------------------------------------- */

/**
 * @brief  Amplifier compression ratio.
 *
 * Maps to bits[1:0] of TPA2028D1 register 0x07.
 */
typedef enum {
    DRIVE_A_2_COMP_1_1 = 0,  /**< 1:1 — compression off */
    DRIVE_A_2_COMP_2_1 = 1,  /**< 2:1 */
    DRIVE_A_2_COMP_4_1 = 2,  /**< 4:1 (default) */
    DRIVE_A_2_COMP_8_1 = 3,  /**< 8:1 */
} drive_a_2_comp_t;

/**
 * @brief  Full AGC/DRC configuration for the TPA2028D1 amplifiers.
 *
 * Pass to tile_drive_a_2_amp_set_agc() to configure all AGC parameters.
 * All fields default to the TPA2028D1 power-on defaults when zeroed.
 */
typedef struct {
    uint8_t compression;     /**< drive_a_2_comp_t. 0 = 1:1 (off). */
    int8_t  fixed_gain_db;   /**< -28 to +30 dB. 0 = 0 dB. */
    uint8_t max_gain_db;     /**< 18 to 30 dB. 0 = 18 dB (use 30 for default). */
    uint8_t limiter_level;   /**< 0–31 (0.5 dB steps from -6.5 dBV). 0 = -6.5 dBV. */
    uint8_t attack;          /**< 0–63 (0.1067 ms per step). 0 = fastest. */
    uint8_t release;         /**< 0–63 (0.0137 s per step). 0 = fastest. */
    uint8_t hold;            /**< 0–63 (0.0137 s per step). 0 = disabled. */
    uint8_t noise_gate;      /**< 0–3 (0=1mV, 1=4mV, 2=10mV, 3=20mV rms). */
} drive_a_2_agc_cfg_t;

/* -------------------------------------------------------------- */
/* Init configuration                                              */
/* -------------------------------------------------------------- */

/**
 * @brief  Optional configuration for tile_drive_a_2_init().
 *
 * Pass NULL for defaults: 1x VDD gain on both channels, 6 dB amp gain.
 */
typedef struct {
    uint8_t gain;            /**< drive_a_2_gain_t. 0 = 1x ext VREF.
                                  Use DRIVE_A_2_GAIN_1X_VDD for VDD ref. */
    int8_t  amp_gain_db;     /**< Amplifier fixed gain in dB. 0 = 0 dB.
                                  Power-on default is 6 dB; set explicitly
                                  if you want a different starting gain. */
} drive_a_2_cfg_t;

/* -------------------------------------------------------------- */
/* Public API — Lifecycle                                          */
/* -------------------------------------------------------------- */

/**
 * @brief  Check whether a DAC63202W is present on the bus.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (see mapping table)
 * @return 1 if device ACKs (I2C) or responds to WHO_AM_I (SPI), 0 otherwise
 */
uint8_t tile_drive_a_2_find(tiles_pal_t *hal, uint8_t instance);

/**
 * @brief  Initialize the Drive.A.2 tile.
 *
 * Resets the DAC, verifies DEVICE-ID, powers up both VOUT channels,
 * configures gain, and (in I2C mode) probes and configures the amplifiers.
 *
 * @param  hal       Platform HAL handle (see core_tiles.h).
 * @param  instance  Instance index (I2C: address variant, SPI: CS index)
 * @param  tile      Pointer to tile handle (populated by this function)
 * @param  cfg       Optional config, or NULL for defaults (1x VDD, 6 dB amp)
 */
void tile_drive_a_2_init(tiles_pal_t *hal, uint8_t instance,
                         tile_t *tile, const drive_a_2_cfg_t *cfg);

/**
 * @brief  Enter low-power sleep.
 *
 * Powers down both DAC VOUT channels (Hi-Z) and puts the amplifiers
 * into software shutdown.
 */
void tile_drive_a_2_sleep(tile_t *tile);

/**
 * @brief  Wake from sleep.
 *
 * Powers up both DAC VOUT channels and re-enables the amplifiers.
 * Restores cached gain settings.
 */
void tile_drive_a_2_wake(tile_t *tile);

/**
 * @brief  Perform a software reset of the DAC.
 *
 * All DAC registers return to defaults. tile->state becomes TILE_STATE_NONE;
 * call init() again to reconfigure.
 */
void tile_drive_a_2_reset(tile_t *tile);

/* -------------------------------------------------------------- */
/* Public API — DAC output                                         */
/* -------------------------------------------------------------- */

/**
 * @brief  Set a DAC channel output by raw 12-bit code (0–4095).
 *
 * @param  tile     Tile handle
 * @param  channel  0 or 1
 * @param  value    12-bit DAC code (0 = 0V, 4095 = full-scale)
 */
void tile_drive_a_2_set(tile_t *tile, uint8_t channel, uint16_t value);

/**
 * @brief  Set a DAC channel output in millivolts.
 *
 * Computes the DAC code from the cached reference voltage and gain.
 *
 * @param  tile     Tile handle
 * @param  channel  0 or 1
 * @param  mv       Desired output in millivolts
 */
void tile_drive_a_2_set_mv(tile_t *tile, uint8_t channel, uint16_t mv);

/**
 * @brief  Read back the current DAC code for a channel.
 *
 * @param  tile     Tile handle
 * @param  channel  0 or 1
 * @return 12-bit DAC code currently loaded
 */
uint16_t tile_drive_a_2_get(tile_t *tile, uint8_t channel);

/* -------------------------------------------------------------- */
/* Public API — DAC configuration                                  */
/* -------------------------------------------------------------- */

/**
 * @brief  Set the voltage output gain for a DAC channel.
 *
 * Automatically enables the internal reference when an internal-reference
 * gain is selected, and updates the cached Vref for set_mv() calculations.
 *
 * @param  tile     Tile handle
 * @param  channel  0 or 1
 * @param  gain     One of the drive_a_2_gain_t values
 */
void tile_drive_a_2_set_gain(tile_t *tile, uint8_t channel,
                             drive_a_2_gain_t gain);

/* -------------------------------------------------------------- */
/* Public API — Waveform generation                                */
/* -------------------------------------------------------------- */

/**
 * @brief  Configure the waveform shape for a channel.
 *
 * Sets FUNC-CONFIG-X in DAC-X-FUNC-CONFIG. The waveform oscillates
 * between DAC-X-MARGIN-LOW and DAC-X-MARGIN-HIGH at the configured
 * slew rate. Call start_waveform() to begin output.
 *
 * @param  tile     Tile handle
 * @param  channel  0 or 1
 * @param  wave     Waveform shape
 */
void tile_drive_a_2_set_waveform(tile_t *tile, uint8_t channel,
                                 drive_a_2_wave_t wave);

/**
 * @brief  Start waveform generation on a channel.
 *
 * @param  tile     Tile handle
 * @param  channel  0 or 1
 */
void tile_drive_a_2_start_waveform(tile_t *tile, uint8_t channel);

/**
 * @brief  Stop waveform generation on a channel.
 *
 * @param  tile     Tile handle
 * @param  channel  0 or 1
 */
void tile_drive_a_2_stop_waveform(tile_t *tile, uint8_t channel);

/* -------------------------------------------------------------- */
/* Public API — Amplifier control                                  */
/* -------------------------------------------------------------- */

/**
 * @brief  Set the amplifier fixed gain.
 *
 * Both TPA2028D1 amplifiers share I2C address 0x58, so this write
 * affects both channels simultaneously.  No-op in SPI mode.
 *
 * @param  tile      Tile handle
 * @param  gain_db   Gain in dB (-28 to +30). Clamped if out of range.
 */
void tile_drive_a_2_amp_set_gain(tile_t *tile, int8_t gain_db);

/**
 * @brief  Read the current amplifier fixed gain.
 *
 * @param  tile  Tile handle
 * @return Gain in dB (-28 to +30), or 0 if amp not available
 */
int8_t tile_drive_a_2_amp_get_gain(tile_t *tile);

/**
 * @brief  Enable the amplifiers (clear software shutdown).
 *
 * No-op in SPI mode.
 */
void tile_drive_a_2_amp_enable(tile_t *tile);

/**
 * @brief  Disable the amplifiers (enter software shutdown).
 *
 * No-op in SPI mode.
 */
void tile_drive_a_2_amp_disable(tile_t *tile);

/**
 * @brief  Configure the full AGC/DRC parameters.
 *
 * Writes all AGC registers (attack, release, hold, fixed gain,
 * limiter, compression, noise gate, max gain).  Affects both amps.
 * No-op in SPI mode.
 *
 * @param  tile  Tile handle
 * @param  cfg   AGC configuration
 */
void tile_drive_a_2_amp_set_agc(tile_t *tile, const drive_a_2_agc_cfg_t *cfg);

/**
 * @brief  Read the amplifier status register.
 *
 * Check bit 3 (FAULT) for short-circuit and bit 2 (Thermal) for
 * over-temperature.  Write 0 to the respective bit to clear.
 *
 * @param  tile  Tile handle
 * @return Raw register 0x01 value, or 0 if amp not available
 */
uint8_t tile_drive_a_2_amp_read_status(tile_t *tile);

/* -------------------------------------------------------------- */
/* Public API — Status                                             */
/* -------------------------------------------------------------- */

/**
 * @brief  Read the DAC GENERAL-STATUS register.
 *
 * Contains DEVICE-ID, VERSION-ID, NVM CRC status, and DAC busy flags.
 *
 * @param  tile  Tile handle
 * @return 16-bit GENERAL-STATUS value
 */
uint16_t tile_drive_a_2_read_status(tile_t *tile);

#endif /* INC_TILE_DRIVE_A_2_H_ */
