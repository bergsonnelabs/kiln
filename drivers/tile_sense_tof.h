/**
 * @file  tile_sense_tof.h
 * @brief TMF8806 time-of-flight distance sensor.
 *
 * Platform-agnostic driver for the AMS/Sciosense TMF8806 direct time-of-flight
 * (dToF) sensor with integrated VCSEL emitter and SPAD detector array.
 *
 * Key specifications:
 *  - Distance measurement up to 5000 mm (short-range, 2.5 m, and 5 m modes)
 *  - 16-bit distance output in millimeters
 *  - Configurable measurement period (30 ms to 2000 ms, or single-shot)
 *  - Configurable iteration count for accuracy vs. speed trade-off
 *  - 6-bit reliability indicator (0 = no object, 63 = highest confidence)
 *  - On-chip factory calibration with host-side storage and reload
 *  - Algorithm state save/restore for ultra-low-power resume
 *  - Embedded 8-bit temperature sensor
 *
 * Quick start:
 * @code
 *   #include "core.h"
 *   #include "core_tiles.h"
 *   #include "tile_sense_tof.h"
 *
 *   tile_t tof;
 *   sense_tof_cfg_t cfg = { .mode = SENSE_TOF_RANGE_2500MM };
 *   tile_sense_tof_init(core_tiles_hal(&core_i2c1), 0, &tof, &cfg);
 *
 *   tile_sense_tof_start(&tof);
 *   // ... poll or wait for interrupt ...
 *   uint16_t dist = tile_sense_tof_get_distance_mm(&tof);
 *
 *   // Single-shot convenience:
 *   sense_tof_result_t res;
 *   tile_sense_tof_measure_single(&tof, &res, 500);
 * @endcode
 *
 * Datasheet: https://ams.com/tmf8806
 *
 * @note All bus I/O is routed through tiles_hal_t function pointers.
 *       This driver contains no platform-specific code.
 */

#ifndef TILE_SENSE_TOF_H
#define TILE_SENSE_TOF_H

#include "tiles.h"

/* ---- Driver version ---- */

#define TILE_SENSE_TOF_VERSION_MAJOR  1
#define TILE_SENSE_TOF_VERSION_MINOR  0
#define TILE_SENSE_TOF_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);

/* ---- Instance mapping ----
 *
 *  Instance  |  I2C address  |  Notes
 * -----------|---------------|----------------------------
 *     0      |     0x41      |  Fixed address (only one)
 */

/* ---- I2C addresses ---- */

#define TMF8806_I2C_ADDR            0x41  /**< Fixed 7-bit I2C address */

/* ---- Register map ---- */

#define TMF8806_REG_APPID           0x00  /**< Application ID (0x80=BL, 0xC0=App0) */
#define TMF8806_REG_APPREV_MAJOR    0x01  /**< Application revision major */
#define TMF8806_REG_APPREQID        0x02  /**< Application request ID (write 0xC0) */
#define TMF8806_REG_CMD_DATA9       0x06  /**< Command payload byte 9 (first byte) */
#define TMF8806_REG_CMD_DATA8       0x07  /**< Command payload byte 8 */
#define TMF8806_REG_CMD_DATA7       0x08  /**< Command payload byte 7 */
#define TMF8806_REG_CMD_DATA6       0x09  /**< Command payload byte 6 */
#define TMF8806_REG_CMD_DATA5       0x0A  /**< Command payload byte 5 */
#define TMF8806_REG_CMD_DATA4       0x0B  /**< Command payload byte 4 */
#define TMF8806_REG_CMD_DATA3       0x0C  /**< Command payload byte 3 */
#define TMF8806_REG_CMD_DATA2       0x0D  /**< Command payload byte 2 */
#define TMF8806_REG_CMD_DATA1       0x0E  /**< Command payload byte 1 */
#define TMF8806_REG_CMD_DATA0       0x0F  /**< Command payload byte 0 */
#define TMF8806_REG_COMMAND         0x10  /**< Command trigger register */
#define TMF8806_REG_PREVIOUS        0x11  /**< Last executed command */
#define TMF8806_REG_APPREV_MINOR    0x12  /**< Application revision minor */
#define TMF8806_REG_APPREV_PATCH    0x13  /**< Application revision patch */
#define TMF8806_REG_STATE           0x1C  /**< Application state (2 = error) */
#define TMF8806_REG_STATUS          0x1D  /**< Result status byte */
#define TMF8806_REG_REG_CONTENTS    0x1E  /**< Register contents type indicator */
#define TMF8806_REG_TID             0x1F  /**< Transaction ID (per-result) */
#define TMF8806_REG_RESULT_NUMBER   0x20  /**< Result counter */
#define TMF8806_REG_RESULT_INFO     0x21  /**< Measurement status + reliability */
#define TMF8806_REG_DISTANCE_LSB    0x22  /**< Distance peak 0 (low byte) */
#define TMF8806_REG_DISTANCE_MSB    0x23  /**< Distance peak 0 (high byte) */
#define TMF8806_REG_SYS_CLOCK_0     0x24  /**< System clock byte 0 (LSB) */
#define TMF8806_REG_SYS_CLOCK_3     0x27  /**< System clock byte 3 (MSB) */
#define TMF8806_REG_STATE_DATA      0x28  /**< Algorithm state data (11 bytes) */
#define TMF8806_REG_TEMPERATURE     0x33  /**< Die temperature (signed 8-bit C) */
#define TMF8806_REG_REF_HITS_0      0x34  /**< Reference hits byte 0 (LSB) */
#define TMF8806_REG_OBJ_HITS_0      0x38  /**< Object hits byte 0 (LSB) */
#define TMF8806_REG_XTALK_LSB       0x3C  /**< Cross-talk count (low byte) */
#define TMF8806_REG_XTALK_MSB       0x3D  /**< Cross-talk count (high byte) */
#define TMF8806_REG_FACTORY_CALIB   0x20  /**< Factory calibration data (14 bytes) */
#define TMF8806_REG_STATE_DATA_WR   0x2E  /**< Algorithm state write (11 bytes) */
#define TMF8806_REG_ENABLE          0xE0  /**< Power control + CPU status */
#define TMF8806_REG_INT_STATUS      0xE1  /**< Interrupt status (write-1-clear) */
#define TMF8806_REG_INT_ENAB        0xE2  /**< Interrupt enable mask */
#define TMF8806_REG_ID              0xE3  /**< Device ID register */
#define TMF8806_REG_REVID           0xE4  /**< Silicon revision ID */

/* ---- Device ID ---- */

#define TMF8806_DEVICE_ID           0x09  /**< Expected ID value (bits 5:0 only) */
#define TMF8806_ID_MASK             0x3F  /**< Mask for valid ID bits */

/* ---- ENABLE register values ---- */

#define TMF8806_ENABLE_PON          0x01  /**< Power-on bit */
#define TMF8806_ENABLE_CPU_READY    0x41  /**< CPU ready + PON */
#define TMF8806_ENABLE_CPU_RESET    0x80  /**< CPU reset bit */

/* ---- APPID values ---- */

#define TMF8806_APPID_BOOTLOADER    0x80  /**< Bootloader is running */
#define TMF8806_APPID_APP0          0xC0  /**< Measurement application running */

/* ---- Command codes ---- */

#define TMF8806_CMD_MEASURE         0x02  /**< Start measurement */
#define TMF8806_CMD_FACTORY_CAL     0x0A  /**< Run factory calibration */
#define TMF8806_CMD_STOP            0xFF  /**< Stop measurement */
#define TMF8806_CMD_SERIAL          0x47  /**< Read serial number */

/* ---- INT_STATUS bit masks ---- */

#define TMF8806_INT_RESULT          0x01  /**< Result interrupt flag */
#define TMF8806_INT_HISTOGRAM       0x02  /**< Histogram interrupt flag */

/* ---- REGISTER_CONTENTS values ---- */

#define TMF8806_CONTENTS_RESULT     0x55  /**< Result data available */
#define TMF8806_CONTENTS_CALIB      0x0A  /**< Calibration data available */
#define TMF8806_CONTENTS_SERIAL     0x47  /**< Serial number available */

/* ---- Calibration data ---- */

#define TMF8806_CALIB_DATA_LEN      14    /**< Factory calibration data length */
#define TMF8806_STATE_DATA_LEN      11    /**< Algorithm state data length */

/* ---- Boot / poll timeouts ---- */

#define TMF8806_BOOT_TIMEOUT_MS     500   /**< Maximum boot sequence wait */
#define TMF8806_CMD_TIMEOUT_MS      1000  /**< Maximum command completion wait */
#define TMF8806_POLL_INTERVAL_MS    2     /**< Polling interval during boot */

/* ---- Enumerations ---- */

/**
 * @brief  Distance mode selection.
 *
 * Controls the maximum detection range and power consumption.
 * Configured via algorithm selection bits in the measurement command.
 */
typedef enum {
    SENSE_TOF_SHORT_RANGE  = 0,  /**< Short range, ~200 mm max, lowest power */
    SENSE_TOF_RANGE_2500MM = 1,  /**< Medium range up to 2500 mm (default) */
    SENSE_TOF_RANGE_5000MM = 2,  /**< Long range up to 5000 mm */
} sense_tof_distance_mode_t;

/* ---- Configuration struct ---- */

/**
 * @brief  Optional init-time configuration for Sense.TOF.
 *
 * Pass NULL to tile_sense_tof_init() for defaults:
 * mode = 2.5 m, period = 30 ms continuous, kilo_iters = 900, threshold = 6.
 *
 * The configuration takes effect when tile_sense_tof_start() is called.
 * Values can be changed between start/stop cycles.
 */
typedef struct {
    uint8_t  mode;        /**< sense_tof_distance_mode_t (default: RANGE_2500MM). */
    uint8_t  period_ms;   /**< Repetition period code. 0x00 = single shot,
                               0x1E = 30 ms (default), 0xFE = 1000 ms,
                               0xFF = 2000 ms. Intermediate values scale
                               linearly in milliseconds. */
    uint16_t kilo_iters;  /**< Iterations in thousands (default: 900). Higher
                               values improve SNR at the cost of measurement
                               time and power. */
    uint8_t  threshold;   /**< Detection threshold, 0-63 (default: 6). Lower
                               values increase sensitivity but may produce
                               more false detections. */
} sense_tof_cfg_t;

/* ---- Result struct ---- */

/**
 * @brief  Measurement result from a single ToF capture.
 *
 * Populated by tile_sense_tof_get_result() or tile_sense_tof_measure_single().
 */
typedef struct {
    uint16_t distance_mm;   /**< Peak distance in millimeters. 0 if no target. */
    uint8_t  status;        /**< Result status. 0x00-0x0F = valid, 0x10+ = error. */
    uint8_t  reliability;   /**< Confidence indicator, 0-63.
                                 0 = no object detected, 63 = highest confidence. */
    int8_t   temperature;   /**< Die temperature in degrees Celsius. */
    uint8_t  result_number; /**< Monotonically incrementing result counter. */
} sense_tof_result_t;

/* ---- Version info struct ---- */

/**
 * @brief  Application firmware version reported by the TMF8806.
 */
typedef struct {
    uint8_t major;  /**< Major version number. */
    uint8_t minor;  /**< Minor version number. */
    uint8_t patch;  /**< Patch version number. */
} sense_tof_version_t;

/* ---- Lifecycle ---- */

/**
 * @brief  Check if a Sense.TOF tile is present on the bus.
 *
 * Probes the I2C address and reads the ID register. The TMF8806 has a
 * single fixed address (0x41), so only instance 0 is valid.
 *
 * @param  hal       Platform HAL handle.
 * @param  instance  Must be 0 (single-address device).
 * @return 1 if device ACKs and ID matches (0x09), 0 otherwise.
 */
uint8_t tile_sense_tof_find(tiles_hal_t *hal, uint8_t instance);

/**
 * @brief  Initialise a Sense.TOF tile.
 *
 * Performs the full TMF8806 boot sequence:
 *  1. Waits for bootloader to enter sleep (ENABLE == 0x00)
 *  2. Wakes the bootloader (PON = 1)
 *  3. Waits for CPU ready (ENABLE == 0x41)
 *  4. Requests App0 measurement application
 *  5. Waits for App0 to start (APPID == 0xC0)
 *  6. Enables result interrupt
 *
 * Does NOT start measurements — call tile_sense_tof_start() after init.
 * Does NOT software-reset to preserve any existing calibration data.
 *
 * @param  hal       Platform HAL handle.
 * @param  instance  Must be 0 (single-address device).
 * @param  tile      Tile handle to initialise.
 * @param  cfg       Configuration (NULL for defaults: 2.5 m, 30 ms, 900k iters).
 */
void tile_sense_tof_init(tiles_hal_t *hal, uint8_t instance,
                         tile_t *tile, const sense_tof_cfg_t *cfg);

/**
 * @brief  Enter standby mode (ENABLE = 0x00).
 *
 * Stops any active measurement and powers down the sensor. Use
 * tile_sense_tof_wake() to resume without full re-initialisation.
 *
 * @param  tile  Initialised tile handle.
 */
void tile_sense_tof_sleep(tile_t *tile);

/**
 * @brief  Resume from standby mode.
 *
 * Re-executes the bootloader wake and App0 request sequence. Does not
 * restart measurements — call tile_sense_tof_start() after waking.
 *
 * @param  tile  Sleeping tile handle.
 */
void tile_sense_tof_wake(tile_t *tile);

/**
 * @brief  Reset the device via the CPU reset bit in ENABLE.
 *
 * Performs a full CPU reset and re-runs the boot sequence. All runtime
 * state including calibration is lost. Call init() again after reset.
 *
 * @param  tile  Tile handle.
 */
void tile_sense_tof_reset(tile_t *tile);

/* ---- Measurement control ---- */

/**
 * @brief  Start continuous or single-shot measurement.
 *
 * Writes the factory calibration data (if loaded), configures the
 * measurement command payload from the current cfg, and issues the
 * measurement command. Results are signaled via the result interrupt.
 *
 * If period_ms == 0x00 in the config, a single measurement is taken.
 * Otherwise measurements repeat at the configured period.
 *
 * @param  tile  Initialised tile handle.
 */
void tile_sense_tof_start(tile_t *tile);

/**
 * @brief  Stop an active measurement.
 *
 * Sends the stop command and waits for the sensor to acknowledge.
 * No-op if no measurement is active.
 *
 * @param  tile  Initialised tile handle.
 */
void tile_sense_tof_stop(tile_t *tile);

/**
 * @brief  Perform a blocking single-shot measurement.
 *
 * Temporarily overrides the repetition period to single-shot mode,
 * starts a measurement, polls for the result interrupt, reads the
 * result, and restores the original period setting.
 *
 * @param  tile        Initialised tile handle.
 * @param  result      Output: measurement result (may be NULL to discard).
 * @param  timeout_ms  Maximum wait time in milliseconds.
 * @return 1 if a valid result was obtained, 0 on timeout or error.
 */
uint8_t tile_sense_tof_measure_single(tile_t *tile, sense_tof_result_t *result,
                                      uint32_t timeout_ms);

/* ---- Result reading ---- */

/**
 * @brief  Read the distance from the most recent result.
 *
 * Returns the peak distance in millimeters. Does not check whether
 * new data is available — call tile_sense_tof_result_ready() first
 * or use tile_sense_tof_get_result() for full status.
 *
 * @param  tile  Initialised tile handle.
 * @return Distance in millimeters, or 0 if no object detected.
 */
uint16_t tile_sense_tof_get_distance_mm(tile_t *tile);

/**
 * @brief  Read the full measurement result.
 *
 * Reads distance, status, reliability, temperature, and result number
 * from the sensor in a single bus transaction. Clears the result
 * interrupt flag after reading.
 *
 * @param  tile    Initialised tile handle.
 * @param  result  Output struct to populate.
 */
void tile_sense_tof_get_result(tile_t *tile, sense_tof_result_t *result);

/**
 * @brief  Check if a new measurement result is available.
 *
 * Reads the INT_STATUS register and checks the result interrupt bit.
 * Does not clear the interrupt — that is done by get_result() or
 * get_distance_mm().
 *
 * @param  tile  Initialised tile handle.
 * @return 1 if a new result is pending, 0 otherwise.
 */
uint8_t tile_sense_tof_result_ready(tile_t *tile);

/* ---- Calibration ---- */

/**
 * @brief  Run the factory calibration procedure.
 *
 * Performs a calibration measurement using the current mode settings.
 * The sensor must be positioned with a known target or open field per
 * the TMF8806 calibration guidelines. Results are stored internally
 * and can be retrieved with tile_sense_tof_get_calibration().
 *
 * This is a blocking call that waits for the calibration to complete.
 *
 * @param  tile        Initialised tile handle.
 * @param  timeout_ms  Maximum wait time in milliseconds.
 * @return 1 if calibration completed successfully, 0 on timeout or error.
 */
uint8_t tile_sense_tof_factory_calibrate(tile_t *tile, uint32_t timeout_ms);

/**
 * @brief  Load factory calibration data into the driver.
 *
 * Stores a 14-byte calibration dataset that will be written to the
 * sensor before each measurement start. Call this after init() to
 * restore calibration from non-volatile storage.
 *
 * @param  tile  Initialised tile handle.
 * @param  data  Pointer to 14-byte calibration data array.
 */
void tile_sense_tof_set_calibration(tile_t *tile, const uint8_t *data);

/**
 * @brief  Read the current factory calibration data from the sensor.
 *
 * Reads 14 bytes of calibration data from the sensor's calibration
 * registers. Typically called after tile_sense_tof_factory_calibrate()
 * to save the data for later reloading via set_calibration().
 *
 * @param  tile  Initialised tile handle.
 * @param  data  Output buffer for 14 bytes of calibration data.
 */
void tile_sense_tof_get_calibration(tile_t *tile, uint8_t *data);

/* ---- Info ---- */

/**
 * @brief  Read the application firmware version.
 *
 * Returns the major, minor, and patch version of the App0 measurement
 * application running on the TMF8806.
 *
 * @param  tile     Initialised tile handle.
 * @param  version  Output struct to populate.
 */
void tile_sense_tof_get_app_version(tile_t *tile, sense_tof_version_t *version);

/**
 * @brief  Read the device serial number.
 *
 * Issues the serial number command (0x47) and reads back the response.
 * The serial number is a 4-byte unique device identifier.
 *
 * @param  tile    Initialised tile handle.
 * @param  serial  Output buffer for 4 bytes of serial data.
 * @return 1 if serial number was read successfully, 0 on error.
 */
uint8_t tile_sense_tof_get_serial_number(tile_t *tile, uint8_t *serial);

/* ---- Runtime configuration ---- */

/**
 * @brief  Change the distance mode on the fly.
 *
 * Stops any active measurement, updates the cached mode, and restarts.
 * If no measurement was running, only updates the config for the next start().
 *
 * @param  tile  Initialised tile handle.
 * @param  mode  New distance mode.
 */
void tile_sense_tof_set_distance_mode(tile_t *tile, sense_tof_distance_mode_t mode);

/**
 * @brief  Change the measurement repetition period on the fly.
 *
 * Stops any active measurement, updates the cached period, and restarts.
 * If no measurement was running, only updates the config for the next start().
 *
 * @param  tile       Initialised tile handle.
 * @param  period_ms  New period code (0x00=single, 0x1E=30ms, 0xFE=1s, 0xFF=2s).
 */
void tile_sense_tof_set_period(tile_t *tile, uint8_t period_ms);

/* ---- Algorithm state (ultra-low-power) ---- */

/**
 * @brief  Save the algorithm state from the sensor.
 *
 * Reads 11 bytes of algorithm state data from the sensor (registers
 * 0x28-0x32). This state should be saved before entering sleep in
 * ultra-low-power mode, and restored after wake via restore_state().
 *
 * Preserving algorithm state across power cycles avoids the ~8 ms
 * re-initialisation penalty and maintains measurement accuracy.
 *
 * @param  tile  Initialised tile handle (measurement should be stopped).
 * @param  data  Output buffer for 11 bytes of state data.
 */
void tile_sense_tof_save_state(tile_t *tile, uint8_t *data);

/**
 * @brief  Restore algorithm state to the sensor.
 *
 * Writes 11 bytes of previously saved algorithm state to the sensor
 * (registers 0x2E-0x38). Call this after wake() and before start()
 * to resume from the saved algorithm state.
 *
 * The calibration data bitmask in the measurement command (cmd_data7)
 * is automatically updated to include algState when state data has
 * been restored.
 *
 * @param  tile  Initialised tile handle (after wake, before start).
 * @param  data  Pointer to 11 bytes of previously saved state data.
 */
void tile_sense_tof_restore_state(tile_t *tile, const uint8_t *data);

#endif /* TILE_SENSE_TOF_H */
