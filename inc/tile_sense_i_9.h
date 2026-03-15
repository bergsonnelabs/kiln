/**
 * @file   tile_sense_i_9.h
 * @brief  9-DOF IMU driver for the Sense.I.9 tile (ICM-20948 + AK09916).
 *
 * Provides accelerometer (±2/4/8/16 G), gyroscope (±250–2000 DPS),
 * magnetometer (3-axis), and temperature readings over I2C.
 *
 * Hardware:
 *   - ICM-20948 at I2C address 0x69 (accel, gyro, temp)
 *   - AK09916 magnetometer at I2C address 0x0C (accessed via I2C bypass)
 *
 * Requires: kiln_hal.h platform abstraction.
 *
 * Quick start:
 * @code
 *   kiln_hal_t hal;
 *   kiln_hal_stm32_init(&hal, &hi2c1);          // or your platform's init
 *
 *   if (tile_sense_i_9_init(&hal)) {
 *       tile_sense_i_9_set_accel_range(ACCEL_2G);
 *       tile_sense_i_9_set_gyro_range(GYRO_250DPS);
 *       tile_sense_i_9_set_mag_mode(MAG_CONTINUOUS_100HZ);
 *
 *       int16_t accel[3], gyro[3], mag[3];
 *       tile_sense_i_9_get_raw_accels(accel);    // [X, Y, Z]
 *       tile_sense_i_9_get_raw_gyros(gyro);      // [X, Y, Z]
 *       tile_sense_i_9_get_raw_mags(mag);        // [X, Y, Z]
 *   }
 * @endcode
 */

#ifndef INC_TILE_SENSE_I_9_H_
#define INC_TILE_SENSE_I_9_H_

#include "kiln_hal.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* I2C addresses                                                   */
/* -------------------------------------------------------------- */

#define ICM20948_I2C_ADDR           0x69
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
#define ICM20948_REG_ACCEL_X_H      0x2D
#define ICM20948_REG_ACCEL_X_L      0x2E
#define ICM20948_REG_ACCEL_Y_H      0x2F
#define ICM20948_REG_ACCEL_Y_L      0x30
#define ICM20948_REG_ACCEL_Z_H      0x31
#define ICM20948_REG_ACCEL_Z_L      0x32
#define ICM20948_REG_GYRO_X_H       0x33
#define ICM20948_REG_GYRO_X_L       0x34
#define ICM20948_REG_GYRO_Y_H       0x35
#define ICM20948_REG_GYRO_Y_L       0x36
#define ICM20948_REG_GYRO_Z_H       0x37
#define ICM20948_REG_GYRO_Z_L       0x38
#define ICM20948_REG_TEMP_H         0x39
#define ICM20948_REG_TEMP_L         0x3A

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
#define AK09916_REG_HXH             0x12
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
 *   ACCEL_2G  → 16384,  ACCEL_4G  → 8192,
 *   ACCEL_8G  → 4096,   ACCEL_16G → 2048
 */
typedef enum {
    ACCEL_2G   = 0x00,
    ACCEL_4G   = 0x02,  /**< bits [2:1] of ACCEL_CONFIG */
    ACCEL_8G   = 0x04,
    ACCEL_16G  = 0x06,
} accel_range_t;

/**
 * @brief  Gyroscope full-scale range.
 *
 * Sensitivity (LSB/°/s) for each range:
 *   GYRO_250DPS  → 131.0,  GYRO_500DPS  → 65.5,
 *   GYRO_1000DPS → 32.8,   GYRO_2000DPS → 16.4
 */
typedef enum {
    GYRO_250DPS  = 0x00,
    GYRO_500DPS  = 0x02,  /**< bits [2:1] of GYRO_CONFIG */
    GYRO_1000DPS = 0x04,
    GYRO_2000DPS = 0x06,
} gyro_range_t;

/**
 * @brief  Magnetometer operating mode.
 *
 * Single-measurement mode requires re-triggering for each read.
 * Continuous modes free-run at the specified rate.
 */
typedef enum {
    MAG_POWER_DOWN       = 0x00,
    MAG_SINGLE           = 0x01,
    MAG_CONTINUOUS_10HZ  = 0x02,
    MAG_CONTINUOUS_20HZ  = 0x04,
    MAG_CONTINUOUS_50HZ  = 0x06,
    MAG_CONTINUOUS_100HZ = 0x08,
} mag_mode_t;

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

/**
 * @brief  Check whether an ICM-20948 is present on the I2C bus.
 *
 * Performs an address-level probe only (no register reads).
 *
 * @param  hal  Platform HAL handle
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_sense_i_9_find(kiln_hal_t* hal);

/**
 * @brief  Initialize the ICM-20948 and AK09916.
 *
 * Performs a soft reset, verifies WHO_AM_I, wakes the device,
 * enables all accel + gyro axes, enables I2C bypass for direct
 * magnetometer access, and starts the magnetometer in continuous
 * 100 Hz mode.
 *
 * @param  hal  Platform HAL handle
 * @return 1 on success, 0 if WHO_AM_I check fails
 *
 * @note   Blocks for ~50 ms during reset. Call once at startup.
 */
uint8_t tile_sense_i_9_init(kiln_hal_t* hal);

/**
 * @brief  Set the accelerometer full-scale range.
 *
 * Takes effect immediately. Does not alter the gyroscope setting.
 *
 * @param  range  One of ACCEL_2G, ACCEL_4G, ACCEL_8G, ACCEL_16G
 */
void tile_sense_i_9_set_accel_range(accel_range_t range);

/**
 * @brief  Set the gyroscope full-scale range.
 *
 * Takes effect immediately. Does not alter the accelerometer setting.
 *
 * @param  range  One of GYRO_250DPS, GYRO_500DPS, GYRO_1000DPS, GYRO_2000DPS
 */
void tile_sense_i_9_set_gyro_range(gyro_range_t range);

/**
 * @brief  Set the magnetometer operating mode.
 *
 * @param  mode  One of the mag_mode_t values
 *
 * @note   Switching modes resets the AK09916 measurement cycle.
 */
void tile_sense_i_9_set_mag_mode(mag_mode_t mode);

/**
 * @brief  Set the accelerometer output data rate.
 *
 * ODR = 1125 / (1 + divider) Hz.  Examples:
 *   divider = 0   → 1125 Hz
 *   divider = 4   →  225 Hz
 *   divider = 10  → ~102 Hz
 *   divider = 44  →   25 Hz
 *
 * @param  divider  11-bit sample rate divider (0–4095)
 */
void tile_sense_i_9_set_accel_odr(uint16_t divider);

/**
 * @brief  Set the gyroscope output data rate.
 *
 * ODR = 1100 / (1 + divider) Hz.  See set_accel_odr for examples.
 *
 * @param  divider  8-bit sample rate divider (0–255)
 */
void tile_sense_i_9_set_gyro_odr(uint8_t divider);

/**
 * @brief  Read raw accelerometer data (3-axis).
 *
 * Returns signed 16-bit ADC counts. Convert to milli-g using the
 * sensitivity for the configured range (e.g. ±2 G → 1 LSB ≈ 0.061 mg).
 *
 * @param  buffer  Output array, minimum 3 × int16_t [X, Y, Z]
 */
void tile_sense_i_9_get_raw_accels(int16_t* buffer);

/**
 * @brief  Read raw gyroscope data (3-axis).
 *
 * Returns signed 16-bit ADC counts. Convert to °/s using the
 * sensitivity for the configured range (e.g. ±250 DPS → 131 LSB/°/s).
 *
 * @param  buffer  Output array, minimum 3 × int16_t [X, Y, Z]
 */
void tile_sense_i_9_get_raw_gyros(int16_t* buffer);

/**
 * @brief  Read raw accelerometer + gyroscope data in a single burst.
 *
 * More efficient than calling get_raw_accels + get_raw_gyros separately
 * (one I2C transaction instead of two). Data is time-coherent.
 *
 * @param  buffer  Output array, minimum 6 × int16_t [AX, AY, AZ, GX, GY, GZ]
 */
void tile_sense_i_9_get_raw_6dof(int16_t* buffer);

/**
 * @brief  Read raw magnetometer data (3-axis).
 *
 * Returns signed 16-bit ADC counts from the AK09916.
 * Sensitivity: 0.15 µT/LSB (all axes).
 *
 * @param  buffer  Output array, minimum 3 × int16_t [X, Y, Z]
 *
 * @note   Automatically reads the ST2 register to release the
 *         magnetometer data lock, enabling the next measurement.
 */
void tile_sense_i_9_get_raw_mags(int16_t* buffer);

/**
 * @brief  Read the on-chip temperature sensor.
 *
 * Convert raw value to °C:  temp_degC = (raw / 333.87) + 21.0
 *
 * @return Raw signed 16-bit temperature value
 */
int16_t tile_sense_i_9_get_temperature(void);

/**
 * @brief  Enter low-power sleep mode.
 *
 * Stops all sensor sampling. Current draw drops to ~8 µA.
 * Call tile_sense_i_9_wake() to resume.
 */
void tile_sense_i_9_sleep(void);

/**
 * @brief  Wake from sleep mode and resume sampling.
 *
 * Restores auto clock selection. Previously configured ranges
 * and ODRs are preserved across sleep/wake cycles.
 */
void tile_sense_i_9_wake(void);

/**
 * @brief  Perform a software reset.
 *
 * Resets all registers to defaults. Blocks for ~50 ms.
 * You must call tile_sense_i_9_init() again after reset.
 */
void tile_sense_i_9_reset(void);

#endif /* INC_TILE_SENSE_I_9_H_ */
