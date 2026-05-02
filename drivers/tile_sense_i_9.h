/**
 * @file   tile_sense_i_9.h
 * @brief  9-DOF IMU driver for the Sense.I.9 tile (rev c).
 *
 * Embeds the TDK InvenSense ICM-20948: 6-DOF IMU (accel + gyro)
 * with co-packaged AK09916 3-DOF magnetometer.
 *
 * Sensor specifications:
 *   - Accelerometer:  16-bit, ±2/4/8/16 G, up to 4.5 kHz ODR
 *   - Gyroscope:      16-bit, ±250/500/1000/2000 DPS, up to 9 kHz ODR
 *   - Magnetometer:   16-bit, ±4900 µT, up to 100 Hz ODR
 *   - Temperature:    on-chip sensor
 *
 * Datasheet: https://www.bergsonne.io/tiles/sense/i9
 *
 * @tessera tile label=Sense.I.9 icon=⊙
 *
 * Quick start:
 * @code
 *   #include "core_tiles.h"
 *
 *   tile_t imu;
 *   tile_sense_i_9_init(core_tiles_pal(&core_i2c1), 0, &imu, NULL);
 *   if (tile_is_ready(&imu)) {
 *       int16_t accel[3], gyro[3], mag[3];
 *       tile_sense_i_9_get_raw_accels(&imu, accel);
 *       tile_sense_i_9_get_raw_gyros(&imu, gyro);
 *       tile_sense_i_9_get_raw_mags(&imu, mag);
 *   }
 * @endcode
 *
 * Two tiles on one bus:
 * @code
 *   tile_t imu_a, imu_b;
 *   tiles_pal_t *hal = core_tiles_pal(&core_i2c1);
 *   tile_sense_i_9_init(hal, 0, &imu_a, NULL);  // pad 2 floating (0x69)
 *   tile_sense_i_9_init(hal, 1, &imu_b, NULL);  // pad 2 grounded (0x68)
 * @endcode
 *
 * Driver gaps (chip capabilities not exposed by this driver):
 *
 * @tessera unsupported severity=common category="DMP3 (Digital Motion Processor)"
 *   ICM-20948 ships with a full DMP3 firmware blob in ROM
 *   (quaternion fusion, gesture detection, pedometer, BAC
 *   classifier, tap / double-tap, significant motion). Driver
 *   doesn't initialize or read DMP — significant chunk of chip
 *   capability missing. Headline feature for fusion-heavy use cases.
 *
 * @tessera unsupported severity=common category="AK09916 FUSE ROM sensitivity adjustment"
 *   Magnetometer ships with per-axis sensitivity-adjustment ASA
 *   codes in FUSE ROM. Calibrated readings = raw × (ASA + 128) / 256.
 *   Driver returns raw counts without applying FUSE ROM correction —
 *   any user trying to do compass-grade work will see per-axis bias.
 *
 * @tessera unsupported severity=advanced category="AK09916 magnetic overflow flag (HOFL)"
 *   Mag readings can saturate near strong fields; the AK09916 raises
 *   HOFL in its status register to flag invalid samples. Driver
 *   doesn't surface the flag — readings during overflow look valid
 *   but aren't.
 *
 * @tessera unsupported severity=common category="Self-test sweep"
 *   Chip has factory self-test capability for accel and gyro
 *   (mechanical self-excitation against stored factory limits).
 *   Driver has no self-test API — relevant for production-line
 *   validation.
 *
 * @tessera unsupported severity=common category="Wake-on-Motion"
 *   Accel can run in low-power mode and wake the chip on motion
 *   above a programmable threshold (WOM_THRESHOLD: 0–255 mg in
 *   4 mg steps). Driver doesn't expose WoM — relevant for
 *   battery-powered always-on motion-detection use cases.
 *
 * @tessera unsupported severity=common category="FIFO"
 *   Chip has a 512+ byte FIFO with watermark / overflow interrupts
 *   and DMP / raw packet formats. Driver only reads sensor registers
 *   directly (no batched / buffered acquisition).
 *
 * @tessera unsupported severity=common category="Interrupt source configuration"
 *   INT pad is wired but the driver doesn't configure INT_PIN_CFG
 *   (edge / level / latch / open-drain) or route specific sources
 *   (data-ready, WoM, FIFO watermark, DMP) to the pin. Pad is
 *   currently dead.
 *
 * @tessera unsupported severity=advanced category="Sensor hub for external aux sensors"
 *   ICM acts as I²C master with 4 slave slots. Driver uses slot 0
 *   for the internal AK09916; slots 1–3 are unused. Hooking external
 *   aux sensors (additional mag, baro, etc.) into the ICM's FIFO
 *   isn't exposed.
 *
 * @tessera unsupported severity=advanced category="FSYNC external-clock / timestamping"
 *   Chip can take an external 31–50 kHz FSYNC input and stamp
 *   samples against it. Tile doesn't expose the FSYNC pad and the
 *   driver has no FSYNC API — relevant for multi-IMU sync.
 *
 * @tessera unsupported severity=advanced category="Alternate bus modes (SPI / I3C)"
 *   Tile JSON straps support I²C (default) or SPI 4-wire on the same
 *   pads (AD0 = MISO, EN = CS). Driver is I²C-only. ICM-20948 also
 *   has I3C electrical compliance but TDK doesn't fully document it;
 *   would need additional bring-up work.
 */

#ifndef INC_TILE_SENSE_I_9_H_
#define INC_TILE_SENSE_I_9_H_

#include "tiles.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* Driver version                                                  */
/* -------------------------------------------------------------- */

#define TILE_SENSE_I_9_VERSION_MAJOR  2
#define TILE_SENSE_I_9_VERSION_MINOR  0
#define TILE_SENSE_I_9_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);  /* requires tiles.h >= 1.0 */

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

/**
 * @brief  Instance-to-address mapping for Sense.I.9.
 *
 * | Instance | ID   | Bus  | Hardware config                    |
 * |----------|------|------|------------------------------------|
 * | 0        | 0x69 | I2C  | Pad 2 (AD0) floating — default    |
 * | 1        | 0x68 | I2C  | Pad 2 (AD0) tied to GND           |
 *
 * @note  Pad 3 (I2C.EN) has a weak pull-up enabling I2C by default.
 *        Grounding pad 3 switches to SPI mode (not yet supported).
 */
#define ICM20948_I2C_ADDR_DEFAULT   0x69
#define ICM20948_I2C_ADDR_ALT       0x68

/** @brief  AK09916 magnetometer address (fixed, accessed via I2C bypass). */
#define AK09916_I2C_ADDR            0x0C

/* -------------------------------------------------------------- */
/* ICM-20948 register map                                          */
/* -------------------------------------------------------------- */

/* Bank selection */
#define ICM20948_BANK_0             0x00
#define ICM20948_BANK_1             0x01
#define ICM20948_BANK_2             0x02
#define ICM20948_BANK_3             0x03
#define ICM20948_REG_BANK_SEL       0x7F

/* Bank 0 registers */
#define ICM20948_REG_WHOAMI         0x00
#define ICM20948_USER_CTRL          0x03
#define ICM20948_REG_PWR_MGMT_1     0x06
#define ICM20948_REG_PWR_MGMT_2     0x07
#define ICM20948_REG_INT_PIN_CFG    0x0F
#define ICM20948_REG_INT_STATUS_1   0x1A
#define ICM20948_REG_ACCEL_X_H      0x2D
#define ICM20948_REG_GYRO_X_H       0x33
#define ICM20948_REG_TEMP_H         0x39

/* Bank 2 registers */
#define ICM20948_REG_GYRO_SMPLRT    0x00
#define ICM20948_REG_GYRO_CONFIG    0x01
#define ICM20948_REG_ACCEL_SMPLRT_H 0x10
#define ICM20948_REG_ACCEL_SMPLRT_L 0x11
#define ICM20948_REG_ACCEL_CONFIG   0x14

/* Chip ID */
#define ICM20948_WHOAMI_DEFAULT     0xEA

/* -------------------------------------------------------------- */
/* AK09916 magnetometer register map                               */
/* -------------------------------------------------------------- */

#define AK09916_REG_WIA2            0x01
#define AK09916_REG_ST1             0x10
#define AK09916_REG_HXL             0x11
#define AK09916_REG_ST2             0x18
#define AK09916_REG_CNTL2           0x31
#define AK09916_REG_CNTL3           0x32

#define AK09916_WHOAMI_DEFAULT      0x09

/* -------------------------------------------------------------- */
/* Configuration enums                                             */
/* -------------------------------------------------------------- */

/**
 * @brief  Accelerometer full-scale range.
 *
 * Sensitivity (LSB/g) for each range:
 *   SENSE_I_9_ACCEL_2G  → 16384,  SENSE_I_9_ACCEL_4G  → 8192,
 *   SENSE_I_9_ACCEL_8G  → 4096,   SENSE_I_9_ACCEL_16G → 2048
 */
typedef enum {
    SENSE_I_9_ACCEL_2G   = 0x00,  /**< +/- 2g */
    SENSE_I_9_ACCEL_4G   = 0x02,  /**< bits [2:1] of ACCEL_CONFIG */
    SENSE_I_9_ACCEL_8G   = 0x04,  /**< +/- 8g */
    SENSE_I_9_ACCEL_16G  = 0x06,  /**< +/- 16g */
} sense_i_9_accel_range_t;

/**
 * @brief  Gyroscope full-scale range.
 *
 * Sensitivity (LSB/°/s) for each range:
 *   SENSE_I_9_GYRO_250DPS  → 131.0,  SENSE_I_9_GYRO_500DPS  → 65.5,
 *   SENSE_I_9_GYRO_1000DPS → 32.8,   SENSE_I_9_GYRO_2000DPS → 16.4
 */
typedef enum {
    SENSE_I_9_GYRO_250DPS  = 0x00,  /**< +/- 250 deg/s */
    SENSE_I_9_GYRO_500DPS  = 0x02,  /**< bits [2:1] of GYRO_CONFIG */
    SENSE_I_9_GYRO_1000DPS = 0x04,  /**< +/- 1000 deg/s */
    SENSE_I_9_GYRO_2000DPS = 0x06,  /**< +/- 2000 deg/s */
} sense_i_9_gyro_range_t;

/**
 * @brief  Magnetometer operating mode.
 *
 * Single-measurement mode requires re-triggering for each read.
 * Continuous modes free-run at the specified rate.
 */
typedef enum {
    SENSE_I_9_MAG_POWER_DOWN       = 0x00,  /**< Power-down */
    SENSE_I_9_MAG_SINGLE           = 0x01,  /**< Single measurement */
    SENSE_I_9_MAG_CONTINUOUS_10HZ  = 0x02,  /**< Continuous at 10 Hz */
    SENSE_I_9_MAG_CONTINUOUS_20HZ  = 0x04,  /**< Continuous at 20 Hz */
    SENSE_I_9_MAG_CONTINUOUS_50HZ  = 0x06,  /**< Continuous at 50 Hz */
    SENSE_I_9_MAG_CONTINUOUS_100HZ = 0x08,  /**< Continuous at 100 Hz */
} sense_i_9_mag_mode_t;

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

/**
 * @brief  Check whether a Sense.I.9 is present on the I2C bus.
 *
 * Performs an address-level probe only (no register reads).
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_sense_i_9_find(tiles_pal_t* hal, uint8_t instance);

/**
 * Optional init config. Pass NULL for defaults.
 * Reserved for future use (e.g., initial range/ODR settings).
 */
typedef struct {
    uint8_t reserved;   /**< Placeholder — no options yet. */
} sense_i_9_cfg_t;

/**
 * @brief  Initialize the ICM-20948 and AK09916.
 *
 * Performs a soft reset, verifies WHO_AM_I, wakes the device,
 * enables all accel + gyro axes, enables I2C bypass for direct
 * magnetometer access, and starts the magnetometer in continuous
 * 100 Hz mode. Pass cfg=NULL for defaults.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0 = default, see mapping table)
 * @param  tile      Pointer to tile handle (populated by this function)
 * @param  cfg       Optional config, or NULL for defaults
 *
 * @note   Blocks for ~50 ms during reset. Call once at startup.
 */
void tile_sense_i_9_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                         const sense_i_9_cfg_t *cfg);

/**
 * @brief  Check if new IMU data is available.
 * @tessera expose category=tile name=data_ready returns=bool
 *
 * Reads the ICM-20948 interrupt status register.
 *
 * @param  tile  Pointer to tile handle
 * @return 1 if new data is available, 0 otherwise
 */
uint8_t tile_sense_i_9_data_ready(tile_t* tile);

/**
 * @brief  Set the accelerometer full-scale range.
 * @tessera expose category=tile name=set_accel_range
 *
 * @param  tile   Pointer to tile handle
 * @param  range  One of the sense_i_9_accel_range_t values
 */
void tile_sense_i_9_set_accel_range(tile_t* tile, sense_i_9_accel_range_t range);

/**
 * @brief  Set the gyroscope full-scale range.
 * @tessera expose category=tile name=set_gyro_range
 *
 * @param  tile   Pointer to tile handle
 * @param  range  One of the sense_i_9_gyro_range_t values
 */
void tile_sense_i_9_set_gyro_range(tile_t* tile, sense_i_9_gyro_range_t range);

/**
 * @brief  Set the magnetometer operating mode.
 * @tessera expose category=tile name=set_mag_mode
 *
 * @param  tile  Pointer to tile handle
 * @param  mode  One of the sense_i_9_mag_mode_t values
 *
 * @note   Switching modes resets the AK09916 measurement cycle.
 */
void tile_sense_i_9_set_mag_mode(tile_t* tile, sense_i_9_mag_mode_t mode);

/**
 * @brief  Set the accelerometer output data rate.
 * @tessera expose category=tile name=set_accel_odr
 *
 * ODR = 1125 / (1 + divider) Hz.  Examples:
 *   divider = 0   → 1125 Hz
 *   divider = 4   →  225 Hz
 *   divider = 10  → ~102 Hz
 *   divider = 44  →   25 Hz
 *
 * @param  tile     Pointer to tile handle
 * @param  divider  11-bit sample rate divider (0–4095)
 */
void tile_sense_i_9_set_accel_odr(tile_t* tile, uint16_t divider);

/**
 * @brief  Set the gyroscope output data rate.
 * @tessera expose category=tile name=set_gyro_odr
 *
 * ODR = 1100 / (1 + divider) Hz.
 *
 * @param  tile     Pointer to tile handle
 * @param  divider  8-bit sample rate divider (0–255)
 */
void tile_sense_i_9_set_gyro_odr(tile_t* tile, uint8_t divider);

/**
 * @brief  Read raw accelerometer data (3-axis).
 * @tessera expose category=tile name=get_raw_accels returns=int[3]
 * @tessera out_buffer buffer type=int16_t length=3
 *
 * Returns signed 16-bit ADC counts. Convert to milli-g using the
 * sensitivity for the configured range (e.g. ±2 G → 1 LSB ≈ 0.061 mg).
 *
 * @param  tile    Pointer to tile handle
 * @param  buffer  Output array, minimum 3 × int16_t [X, Y, Z]
 */
void tile_sense_i_9_get_raw_accels(tile_t* tile, int16_t* buffer);

/**
 * @brief  Read raw gyroscope data (3-axis).
 * @tessera expose category=tile name=get_raw_gyros returns=int[3]
 * @tessera out_buffer buffer type=int16_t length=3
 *
 * Returns signed 16-bit ADC counts. Convert to °/s using the
 * sensitivity for the configured range (e.g. ±250 DPS → 131 LSB/°/s).
 *
 * @param  tile    Pointer to tile handle
 * @param  buffer  Output array, minimum 3 × int16_t [X, Y, Z]
 */
void tile_sense_i_9_get_raw_gyros(tile_t* tile, int16_t* buffer);

/**
 * @brief  Read raw accelerometer + gyroscope data in a single burst.
 * @tessera expose category=tile name=get_raw_6dof returns=int[6]
 * @tessera out_buffer buffer type=int16_t length=6
 *
 * More efficient than calling get_raw_accels + get_raw_gyros separately
 * (one I2C transaction instead of two). Data is time-coherent.
 *
 * @param  tile    Pointer to tile handle
 * @param  buffer  Output array, minimum 6 × int16_t [AX, AY, AZ, GX, GY, GZ]
 */
void tile_sense_i_9_get_raw_6dof(tile_t* tile, int16_t* buffer);

/**
 * @brief  Read raw magnetometer data (3-axis).
 *
 * Returns signed 16-bit ADC counts from the AK09916.
 * Sensitivity: 0.15 µT/LSB (all axes).
 *
 * @tessera expose category=tile name=get_raw_mags returns=int[3]
 * @tessera out_buffer buffer type=int16_t length=3
 * @param  tile    Pointer to tile handle
 * @param  buffer  Output array, minimum 3 × int16_t [X, Y, Z]
 *
 * @note   Automatically reads the ST2 register to release the
 *         magnetometer data lock, enabling the next measurement.
 */
void tile_sense_i_9_get_raw_mags(tile_t* tile, int16_t* buffer);

/**
 * @brief  Read the on-chip temperature sensor.
 * @tessera expose category=tile name=get_temperature returns=int
 *
 * Convert raw value to °C:  temp_degC = (raw / 333.87) + 21.0
 *
 * @param  tile  Pointer to tile handle
 * @return Raw signed 16-bit temperature value
 */
int16_t tile_sense_i_9_get_temperature(tile_t* tile);

/**
 * @brief  Enter low-power sleep mode.
 * @tessera expose category=tile name=sleep
 *
 * Stops all sensor sampling. Current draw drops to ~8 µA.
 * Call tile_sense_i_9_wake() to resume.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_sense_i_9_sleep(tile_t* tile);

/**
 * @brief  Wake from sleep mode and resume sampling.
 * @tessera expose category=tile name=wake
 *
 * Restores auto clock selection. Previously configured ranges
 * and ODRs are preserved across sleep/wake cycles.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_sense_i_9_wake(tile_t* tile);

/**
 * @brief  Perform a software reset.
 *
 * Resets all registers to defaults. Blocks for ~50 ms.
 * You must call tile_sense_i_9_init() again after reset.
 *
 * @tessera expose category=tile name=reset
 * @param  tile  Pointer to tile handle
 */
void tile_sense_i_9_reset(tile_t* tile);

#endif /* INC_TILE_SENSE_I_9_H_ */
