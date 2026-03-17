/**
 * @file   tile_drive_p.h
 * @brief  Piezoelectric haptic driver for the Drive.P tile (rev a).
 *
 * Datasheet: https://www.bergsonne.io/tiles/drive/p
 * JSON API:  https://www.bergsonne.io/api/tile-json?family=Drive&name=P&rev=a
 *
 * The Drive.P embeds the Boréas Technologies BOS1921, a piezoelectric
 * driver with integrated high-voltage boost (190 Vpp differential),
 * waveform synthesizer, 1024-sample FIFO, and piezo sensing.
 *
 * Key specifications:
 *   - Output:      190 Vpp differential, up to 820 nF capacitive load
 *   - Sensing:     7.6 mV resolution (fine), 54.5 mV (coarse)
 *   - Play modes:  Direct, FIFO, RAM, RAM Synth
 *   - Sample rate:  8 ksps – 1024 ksps (configurable)
 *   - Chip ID:     0x0781 (lower 12 bits of default read)
 *
 * IC datasheet:
 *   https://mosaic-component-datasheets.s3.eu-north-1.amazonaws.com/5/Bor_as_Technologies-BOS1921.pdf
 *
 * Tile hardware (T44-10 package, 4.0 × 4.0 × 1.8 mm):
 *   - Pad 1:   GND
 *   - Pad 2:   Unassigned
 *   - Pad 3:   GPIO — digital I/O
 *   - Pad 4:   I2C.CLK / I3C.CLK (external pull-up required without Core)
 *   - Pad 5:   I2C.DAT / I3C.DAT (external pull-up required without Core)
 *   - Pad 6:   Unassigned
 *   - Pad 7:   OUT+ — piezo drive output
 *   - Pad 8:   OUT− — piezo drive output
 *   - Pad 9:   V_DRIVE — boost supply (3.0–5.5 V)
 *   - Pad 10:  V+ — system supply (1.8–5.5 V)
 *
 * I2C: 7-bit address, 8-bit register, 16-bit data (big-endian), up to 1 MHz.
 *
 * Requires: kiln_hal.h platform abstraction.
 *
 * Quick start:
 * @code
 *   kiln_hal_t hal;
 *   kiln_hal_stm32_init(&hal, &hi2c1);
 *
 *   if (tile_drive_p_init(&hal, BOS1921_I2C_ADDR_DEFAULT)) {
 *       // Start haptic playback via FIFO
 *       tile_drive_p_set_mode(DRIVE_P_MODE_PLAY_FIFO);
 *       tile_drive_p_write_fifo(0x7FFF);  // positive peak
 *       tile_drive_p_write_fifo(0x8001);  // negative peak
 *
 *       // Or read piezo force sensing
 *       tile_drive_p_set_mode(DRIVE_P_MODE_SENSE_FINE);
 *       uint16_t force = tile_drive_p_read_sense();
 *   }
 * @endcode
 */

#ifndef INC_TILE_DRIVE_P_H_
#define INC_TILE_DRIVE_P_H_

#include "kiln_hal.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* I2C addresses                                                   */
/* -------------------------------------------------------------- */

/** @brief  Default I2C address (7-bit). */
#define BOS1921_I2C_ADDR_DEFAULT    0x44

/* -------------------------------------------------------------- */
/* BOS1921 register map                                            */
/* -------------------------------------------------------------- */

#define BOS1921_REG_REFERENCE       0x00  /**< Waveform reference / FIFO write */
#define BOS1921_REG_CONFIG          0x05  /**< Main configuration register */
// CONFIG bit layout (16-bit, big-endian):
// Bit 15      ONCOMP    — activate sensing comparator
// Bit 14      AUTO      — trigger auto play
// Bit 13      SENSE     — enable piezo sensing
// Bit 12      GAINS     — sensing resolution (0 = 54.5 mV/LSB; 1 = 7.6 mV/LSB)
// Bit 11      GAIND     — output voltage range (0 = ±95 V; 1 = ±13.28 V)
// Bit 10:9    PLAY_MODE — (0=direct; 1=FIFO; 2=RAM; 3=RAM Synth)
// Bit 8       RET       — retention during sleep (0=enabled; 1=disabled)
// Bit 7       SYNC      — multi-chip sync (0=disable; 1=enable)
// Bit 6       RST       — software reset (0=normal; 1=reset)
// Bit 5       POL_SENSE — sensing polarity to VDD (0=OUT−; 1=OUT+)
// Bit 4       OE        — output enable (0=disable; 1=enable)
// Bit 3       DS        — deep sleep when not playing (0=idle; 1=sleep)
// Bit 2:0     PLAY_SRATE— sample rate (0=1024 ksps; ... 7=8 ksps)

#define BOS1921_REG_PARCAP          0x06  /**< Parasitic capacitance trim */
#define BOS1921_REG_SUP_RISE        0x07  /**< Supply rise time */
#define BOS1921_REG_COMM            0x0B  /**< Communication / return register select */

#define BOS1921_REG_IC_STATUS       0x10  /**< IC status register */
// IC_STATUS bit layout (16-bit):
// Bit 9:8     STATE   — (0=idle; 1=calibrating; 2=running; 3=error)
// Bit 7       OVV     — overvoltage fault
// Bit 6       OCT     — overtemperature fault
// Bit 5       MXPWR   — max power/current warning
// Bit 4       IDAC    — current detection fault (requires reset)
// Bit 3       UVLO    — VDD under-voltage fault
// Bit 2       SC      — piezo short circuit fault
// Bit 1       FULL    — FIFO full
// Bit 0       PLAYST  — play status (mode-dependent)

#define BOS1921_REG_SENSE_VAL       0x18  /**< Sensed piezo voltage */
#define BOS1921_REG_CHIP_ID         0x1E  /**< Chip identification */

/** @brief  Expected lower 12 bits of CHIP_ID register. */
#define BOS1921_CHIP_ID_DEFAULT     0x0781

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
    DRIVE_P_MODE_IDLE         = 0,  /**< Output disabled, status readback */
    DRIVE_P_MODE_SENSE_FINE   = 1,  /**< Piezo sensing, 7.6 mV/LSB resolution */
    DRIVE_P_MODE_SENSE_COARSE = 2,  /**< Piezo sensing, 54.5 mV/LSB resolution */
    DRIVE_P_MODE_PLAY_DIRECT  = 3,  /**< Direct waveform output */
    DRIVE_P_MODE_PLAY_FIFO    = 4,  /**< FIFO-buffered playback, 1024 sps */
    DRIVE_P_MODE_PLAY_RAM_SYNTH = 5, /**< RAM Synthesis waveform playback */
} drive_p_mode_t;

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

/**
 * @brief  Check whether a BOS1921 is present on the I2C bus.
 *
 * Performs an address-level probe only (no register reads).
 *
 * @param  hal   Platform HAL handle
 * @param  addr  7-bit I2C address (BOS1921_I2C_ADDR_DEFAULT)
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_drive_p_find(kiln_hal_t* hal, uint8_t addr);

/**
 * @brief  Initialize the BOS1921 piezoelectric driver.
 *
 * Wakes the device, performs a software reset, and verifies the
 * chip ID register. After init the device is in idle mode.
 *
 * The selected HAL and address are stored internally for all
 * subsequent calls.
 *
 * @param  hal   Platform HAL handle
 * @param  addr  7-bit I2C address (BOS1921_I2C_ADDR_DEFAULT)
 * @return 1 on success, 0 if chip ID check fails
 */
uint8_t tile_drive_p_init(kiln_hal_t* hal, uint8_t addr);

/**
 * @brief  Switch the active HAL and address context.
 *
 * When multiple Drive.P tiles share a driver (e.g. left and right
 * haptic actuators), call this to redirect all subsequent API calls
 * to a different tile without re-running the full init sequence.
 *
 * @param  hal   Platform HAL handle for the target bus
 * @param  addr  7-bit I2C address of the target device
 */
void tile_drive_p_select(kiln_hal_t* hal, uint8_t addr);

/**
 * @brief  Perform a software reset.
 *
 * Sets the RST bit in the CONFIG register. All configuration is
 * lost. You must call tile_drive_p_init() again after reset.
 */
void tile_drive_p_reset(void);

/**
 * @brief  Set the operating mode.
 *
 * Configures the CONFIG register for the requested mode and sets
 * the appropriate return register (IC_STATUS or SENSE_VAL).
 *
 * @param  mode  One of the drive_p_mode_t values
 */
void tile_drive_p_set_mode(drive_p_mode_t mode);

/**
 * @brief  Read the current return register value.
 *
 * The return register depends on the active mode:
 *   - IDLE / PLAY modes: IC_STATUS
 *   - SENSE modes:       SENSE_VAL
 *
 * @return 16-bit value from the currently selected return register
 */
uint16_t tile_drive_p_read(void);

/**
 * @brief  Read the sensed piezo voltage.
 *
 * Must be in SENSE_FINE or SENSE_COARSE mode. Returns the raw ADC
 * value from the BOS1921 sensing circuitry, sign-extended from the
 * native 12-bit two's complement format to 16-bit.
 *
 * Convert to voltage:
 *   - Fine mode:   voltage = raw × 7.6 mV
 *   - Coarse mode: voltage = raw × 54.5 mV
 *
 * @return Signed 16-bit sense value (−2048 to +2047)
 */
int16_t tile_drive_p_read_sense(void);

/**
 * @brief  Read the IC status register.
 *
 * @return 16-bit IC_STATUS value (see register bit layout above)
 */
uint16_t tile_drive_p_read_status(void);

/**
 * @brief  Write a sample to the FIFO.
 *
 * Used in PLAY_FIFO mode. Write signed 16-bit samples representing
 * the desired output waveform. The BOS1921 plays them at the
 * configured sample rate (8 ksps default in PLAY_FIFO mode).
 *
 * @param  sample  Signed 16-bit waveform sample
 */
void tile_drive_p_write_fifo(int16_t sample);

/**
 * @brief  Write a raw 16-bit value to any BOS1921 register.
 *
 * Low-level register access for advanced configuration.
 *
 * @param  reg    8-bit register address
 * @param  value  16-bit value (sent big-endian on the wire)
 */
void tile_drive_p_write_reg(uint8_t reg, uint16_t value);

/**
 * @brief  Write a multi-word WFS command to the BOS1921.
 *
 * Used for RAM Synthesis mode. Writes an array of 16-bit words
 * to the REFERENCE register in a single I2C transaction. The
 * first word is typically a WFS command (RAM_ACCESS or
 * RAM_SYNTHESIS), followed by address and data words.
 *
 * @param  words  Array of 16-bit words (big-endian on wire)
 * @param  count  Number of words (max 8)
 */
void tile_drive_p_wfs_write(const uint16_t* words, uint16_t count);

/**
 * @brief  Enter low-power sleep mode.
 *
 * Sets the DS bit in CONFIG. The boost converter shuts down.
 * Call tile_drive_p_set_mode() to resume operation.
 */
void tile_drive_p_sleep(void);

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
#define BOS_STATUS_FAULT_MASK       0x00FC  /* OVV|OCT|MXPWR|IDAC|UVLO|SC */

/**
 * @brief  Check the BOS1921 status and recover from error/fault states.
 *
 * Reads IC_STATUS. If the device is in ERROR state or any fault bits
 * are set, cycles through IDLE → the given target mode to clear faults.
 *
 * The currently selected HAL/address context is used (set via
 * tile_drive_p_select() before calling).
 *
 * @param  restore_mode  Mode to re-enter after recovery (e.g. PLAY_FIFO)
 * @return 1 if recovery was performed, 0 if device was healthy
 */
uint8_t tile_drive_p_check_and_recover(drive_p_mode_t restore_mode);

#endif /* INC_TILE_DRIVE_P_H_ */
