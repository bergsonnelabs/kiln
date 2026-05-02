/**
 * @file   tile_sense_i_9.h
 * @brief  9-DOF IMU driver for the Sense.I.9 tile (rev c).
 * @version 3.0.0
 *
 * Embeds the TDK InvenSense ICM-20948: 6-DOF IMU (accel + gyro)
 * with co-packaged AK09916 3-DOF magnetometer (accessed via the
 * ICM's I2C bypass).
 *
 * Sensor specifications:
 *   - Accelerometer:  16-bit, ±2/4/8/16 G, up to 4.5 kHz ODR
 *   - Gyroscope:      16-bit, ±250/500/1000/2000 DPS, up to 9 kHz ODR
 *   - Magnetometer:   16-bit, ±4900 µT, up to 100 Hz ODR (0.15 µT/LSB)
 *   - Temperature:    on-chip sensor
 *   - FIFO:           512-byte buffer, accel + gyro + temp packets
 *   - Wake-on-Motion: 4 mg/LSB threshold, INT routable
 *
 * Datasheet: https://www.bergsonne.io/tiles/sense/i9
 *
 * @tessera tile label=Sense.I.9 icon=⊙
 * @tessera event name=data_ready mask=ICM20948_INT_RAW_DATA_RDY
 * @tessera event name=fifo_watermark mask=ICM20948_INT_FIFO_WM
 * @tessera event name=fifo_overflow mask=ICM20948_INT_FIFO_OVF
 * @tessera event name=wake_on_motion mask=ICM20948_INT_WOM
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
 *   classifier, tap / double-tap, significant motion). DMP3 firmware
 *   loading and output parsing is a substantial lift — deferred to
 *   a dedicated future session. Tracked separately from this
 *   coverage pass.
 *
 * @tessera unsupported severity=niche category="AK09916 FUSE ROM sensitivity adjustment"
 *   Not applicable to this chip. Earlier AKM parts (e.g. AK8963) shipped
 *   per-axis ASA codes in FUSE ROM that required `raw × (ASA + 128) / 256`
 *   correction. The AK09916 in the ICM-20948 has no FUSE ROM and no ASA
 *   registers (datasheet rev 015007392-E-02 §11) — sensitivity is
 *   factory-trimmed to a fixed 0.15 µT/LSB across all axes. Driver
 *   correctly returns raw counts at that scale; no per-axis correction
 *   is needed or possible. Annotation kept to document the verification.
 *
 * @tessera unsupported severity=advanced category="Sensor hub for external aux sensors"
 *   Architecturally gated. Driver currently uses INT_PIN_CFG.BYPASS_EN
 *   to expose the AK09916 directly on the host I2C bus, which is
 *   simpler and faster than routing reads through the ICM's I2C master.
 *   Closing this gap means moving AK09916 access into the master (slot 0)
 *   and exposing slots 1–3 for user-attached aux sensors — a structural
 *   rework that touches `get_raw_mags()`, init sequencing, and FIFO
 *   integration. Defer to a followup pass.
 *
 * @tessera unsupported severity=advanced category="FSYNC external-clock / timestamping"
 *   Hardware-gated. Chip can take a 31–50 kHz FSYNC input and stamp
 *   samples against external timing. The Sense.I.9-c tile does not
 *   expose the FSYNC pin on any pad (verified in tile JSON: pads 6, 7,
 *   and 8 carry no function). Closing this gap requires a tile
 *   hardware revision.
 *
 * @tessera unsupported severity=advanced category="Alternate bus modes (SPI / I3C)"
 *   Ecosystem-gated. Tile JSON straps support I²C (default) or SPI 4-wire
 *   on the same pads (AD0 = MISO, EN = CS), and the ICM-20948 is also
 *   I3C electrically compliant. The driver framework currently uses
 *   tiles_pal I²C calls only; closing requires extending the bus
 *   abstraction (see Sense.I.6P6 v1.0+ for the SPI pattern). Defer to
 *   a multi-bus driver framework pass.
 */

#ifndef INC_TILE_SENSE_I_9_H_
#define INC_TILE_SENSE_I_9_H_

#include "tiles.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* Driver version                                                  */
/* -------------------------------------------------------------- */

#define TILE_SENSE_I_9_VERSION_MAJOR  3
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
#define ICM20948_REG_INT_ENABLE     0x10  /**< WoM / DMP / I2C master */
#define ICM20948_REG_INT_ENABLE_1   0x11  /**< RAW_DATA_0_RDY_EN */
#define ICM20948_REG_INT_ENABLE_2   0x12  /**< FIFO_OVERFLOW_EN[4:0] */
#define ICM20948_REG_INT_ENABLE_3   0x13  /**< FIFO_WM_EN[4:0] */
#define ICM20948_REG_INT_STATUS     0x19
#define ICM20948_REG_INT_STATUS_1   0x1A
#define ICM20948_REG_INT_STATUS_2   0x1B  /**< FIFO_OVERFLOW_INT[4:0] */
#define ICM20948_REG_INT_STATUS_3   0x1C  /**< FIFO_WM_INT[4:0] */
#define ICM20948_REG_ACCEL_X_H      0x2D
#define ICM20948_REG_GYRO_X_H       0x33
#define ICM20948_REG_TEMP_H         0x39
#define ICM20948_REG_FIFO_EN_1      0x66
#define ICM20948_REG_FIFO_EN_2      0x67
#define ICM20948_REG_FIFO_RST       0x68
#define ICM20948_REG_FIFO_MODE      0x69
#define ICM20948_REG_FIFO_COUNTH    0x70
#define ICM20948_REG_FIFO_COUNTL    0x71
#define ICM20948_REG_FIFO_R_W       0x72

/* Bank 1 registers (self-test reference values) */
#define ICM20948_B1_SELF_TEST_X_GYRO  0x02
#define ICM20948_B1_SELF_TEST_Y_GYRO  0x03
#define ICM20948_B1_SELF_TEST_Z_GYRO  0x04
#define ICM20948_B1_SELF_TEST_X_ACCEL 0x0E
#define ICM20948_B1_SELF_TEST_Y_ACCEL 0x0F
#define ICM20948_B1_SELF_TEST_Z_ACCEL 0x10

/* Bank 2 registers */
#define ICM20948_REG_GYRO_SMPLRT      0x00
#define ICM20948_REG_GYRO_CONFIG      0x01
#define ICM20948_REG_ACCEL_SMPLRT_H   0x10
#define ICM20948_REG_ACCEL_SMPLRT_L   0x11
#define ICM20948_REG_ACCEL_INTEL_CTRL 0x12  /**< WoM enable + mode */
#define ICM20948_REG_ACCEL_WOM_THR    0x13  /**< WoM threshold (4 mg/LSB) */
#define ICM20948_REG_ACCEL_CONFIG     0x14
#define ICM20948_REG_ACCEL_CONFIG_2   0x15  /**< Self-test enables + averaging */

/* USER_CTRL bits (bank 0, 0x03) */
#define ICM20948_UC_FIFO_EN           (1 << 6)
#define ICM20948_UC_I2C_MST_EN        (1 << 5)
#define ICM20948_UC_I2C_IF_DIS        (1 << 4)
#define ICM20948_UC_DMP_RST           (1 << 3)
#define ICM20948_UC_SRAM_RST          (1 << 2)
#define ICM20948_UC_I2C_MST_RST       (1 << 1)

/* INT_ENABLE bits */
#define ICM20948_INTE_REG_WOF_EN      (1 << 7)
#define ICM20948_INTE_WOM_INT_EN      (1 << 3)
#define ICM20948_INTE_PLL_RDY_EN      (1 << 2)
#define ICM20948_INTE_DMP_INT1_EN     (1 << 1)
#define ICM20948_INTE_I2C_MST_INT_EN  (1 << 0)

/* INT_STATUS event masks (mapped to single byte for tessera events) */
#define ICM20948_INT_RAW_DATA_RDY     (1 << 0)  /**< INT_STATUS_1 bit 0 */
#define ICM20948_INT_FIFO_OVF         (1 << 1)  /**< INT_STATUS_2 any bit (collapsed) */
#define ICM20948_INT_FIFO_WM          (1 << 2)  /**< INT_STATUS_3 any bit (collapsed) */
#define ICM20948_INT_WOM              (1 << 3)  /**< INT_STATUS bit 3 */

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

/* AK09916 ST2 bits */
#define AK09916_ST2_HOFL            (1 << 3)  /**< Magnetic overflow flag */

/* AK09916 self-test mode (CNTL2 = 0x10) */
#define AK09916_MODE_SELF_TEST      0x10

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

/**
 * @brief  INT pin configuration flags. OR together for `int_config`.
 *
 * Active-low + open-drain + latched is typical for shared INT lines.
 */
typedef enum {
    SENSE_I_9_INT_ACTIVE_HIGH  = 0x00,  /**< INT pin active high (default) */
    SENSE_I_9_INT_ACTIVE_LOW   = 0x80,  /**< INT pin active low */
    SENSE_I_9_INT_PUSH_PULL    = 0x00,  /**< Push-pull driver */
    SENSE_I_9_INT_OPEN_DRAIN   = 0x40,  /**< Open-drain driver */
    SENSE_I_9_INT_PULSED       = 0x00,  /**< 50 µs pulse (default) */
    SENSE_I_9_INT_LATCHED      = 0x20,  /**< Held until status read */
    SENSE_I_9_INT_ANYRD_CLEAR  = 0x10,  /**< Clear status on any read */
} sense_i_9_int_flags_t;

/**
 * @brief  Wake-on-Motion compare mode (ACCEL_INTEL_MODE_INT).
 */
typedef enum {
    SENSE_I_9_WOM_VS_INITIAL  = 0x00,  /**< Compare against first sample */
    SENSE_I_9_WOM_VS_PREVIOUS = 0x01,  /**< Compare against previous sample */
} sense_i_9_wom_mode_t;

/**
 * @brief  FIFO operating mode.
 */
typedef enum {
    SENSE_I_9_FIFO_STREAM   = 0x00,  /**< Overwrite oldest data when full */
    SENSE_I_9_FIFO_SNAPSHOT = 0x1F,  /**< Stop accepting writes when full */
} sense_i_9_fifo_mode_t;

/**
 * @brief  Standard FIFO packet (12 bytes: accel + gyro).
 *
 * Populated by tile_sense_i_9_fifo_read_packet() when the FIFO was
 * configured for accel + gyro (the default packet layout).
 */
typedef struct {
    int16_t accel[3];   /**< X, Y, Z accel in raw ADC counts */
    int16_t gyro[3];    /**< X, Y, Z gyro in raw ADC counts */
} sense_i_9_fifo_packet_t;

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
 * Sensitivity: 0.15 µT/LSB on all axes (factory-trimmed; the AK09916
 * has no per-axis ASA / FUSE ROM correction — see datasheet §11).
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
 * @brief  Check whether the most recent magnetometer reading overflowed.
 * @tessera expose category=tile name=mag_overflowed returns=bool
 *
 * The AK09916 raises HOFL in ST2 when the sum |HX|+|HY|+|HZ| exceeds
 * 4912 µT (the chip's measurement range). Readings during overflow
 * appear valid in the data registers but are not — for compass-grade
 * work, discard samples where this returns 1.
 *
 * Reading this releases the AK09916 data-lock the same way
 * tile_sense_i_9_get_raw_mags() does, so it can be used standalone
 * after a non-locking peek.
 *
 * @param  tile  Pointer to tile handle
 * @return 1 if HOFL was set after the most recent measurement, 0 otherwise
 */
uint8_t tile_sense_i_9_mag_overflowed(tile_t* tile);

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

/* ================================================================
 * Interrupt source configuration (INT_PIN_CFG + INT_ENABLE_x)
 * ================================================================ */

/**
 * @brief  Configure the INT pin electrical behaviour.
 *
 * @tessera expose category=tile name=int_config
 *
 * Sets polarity (active low/high), drive (push-pull/open-drain),
 * mode (pulsed/latched), and the auto-clear-on-any-read bit.
 * OR together flags from sense_i_9_int_flags_t.
 *
 * @note  INT_PIN_CFG bit 1 (BYPASS_EN) is preserved by this call —
 *        the driver needs bypass mode to talk to the AK09916.
 *
 * @param  tile   Initialized tile handle
 * @param  flags  OR of SENSE_I_9_INT_* flags
 */
void tile_sense_i_9_int_config(tile_t* tile, uint8_t flags);

/**
 * @brief  Route the data-ready interrupt to the INT pin.
 *
 * @tessera expose category=tile name=int_data_ready
 * @param  tile     Initialized tile handle
 * @param  enabled  1 to enable, 0 to disable
 */
void tile_sense_i_9_int_data_ready(tile_t* tile, uint8_t enabled);

/**
 * @brief  Route the wake-on-motion interrupt to the INT pin.
 *
 * @tessera expose category=tile name=int_wom
 * @param  tile     Initialized tile handle
 * @param  enabled  1 to enable, 0 to disable
 */
void tile_sense_i_9_int_wom(tile_t* tile, uint8_t enabled);

/**
 * @brief  Route the FIFO overflow interrupt to the INT pin.
 *
 * @tessera expose category=tile name=int_fifo_overflow
 * @param  tile     Initialized tile handle
 * @param  enabled  1 to enable for any sensor, 0 to disable all
 */
void tile_sense_i_9_int_fifo_overflow(tile_t* tile, uint8_t enabled);

/**
 * @brief  Route the FIFO watermark interrupt to the INT pin.
 *
 * @tessera expose category=tile name=int_fifo_watermark
 * @param  tile     Initialized tile handle
 * @param  enabled  1 to enable for any sensor, 0 to disable all
 */
void tile_sense_i_9_int_fifo_watermark(tile_t* tile, uint8_t enabled);

/**
 * @brief  Read INT_STATUS (WoM / DMP / I2C-master / PLL-ready) and clear it.
 *
 * @tessera expose category=tile name=get_int_status returns=int
 * @return 8-bit raw register value (use ICM20948_INT_WOM etc. masks)
 */
uint8_t tile_sense_i_9_get_int_status(tile_t* tile);

/**
 * @brief  Read INT_STATUS_2 (FIFO overflow per sensor) and clear it.
 *
 * @tessera expose category=tile name=get_int_status_fifo_ovf returns=int
 * @return 5-bit FIFO_OVERFLOW_INT[4:0]; non-zero means overflow occurred
 */
uint8_t tile_sense_i_9_get_int_status_fifo_overflow(tile_t* tile);

/**
 * @brief  Read INT_STATUS_3 (FIFO watermark per sensor) and clear it.
 *
 * @tessera expose category=tile name=get_int_status_fifo_wm returns=int
 * @return 5-bit FIFO_WM_INT[4:0]; non-zero means watermark crossed
 */
uint8_t tile_sense_i_9_get_int_status_fifo_watermark(tile_t* tile);

/* ================================================================
 * Wake-on-Motion
 * ================================================================ */

/**
 * @brief  Configure Wake-on-Motion threshold and compare mode.
 *
 * @tessera expose category=tile name=wom_config
 *
 * The chip-wide threshold is one 8-bit value at 4 mg/LSB (range
 * 0–1020 mg). Compared independently against |X|, |Y|, |Z|; any
 * axis crossing the threshold raises WOM_INT.
 *
 * @note  Requires accel running. Mag and gyro can be off.
 *
 * @param  tile     Initialized tile handle
 * @param  thr_mg   Threshold in mg (clamped to 1020 mg / 0xFF LSB)
 * @param  mode     Compare against initial sample or previous sample
 */
void tile_sense_i_9_wom_config(tile_t* tile, uint16_t thr_mg,
                               sense_i_9_wom_mode_t mode);

/**
 * @brief  Enable Wake-on-Motion logic.
 *
 * @tessera expose category=tile name=wom_enable
 *
 * Call after wom_config(). Routes to the INT pin only if int_wom()
 * was also enabled.
 *
 * @param  tile  Initialized tile handle
 */
void tile_sense_i_9_wom_enable(tile_t* tile);

/**
 * @brief  Disable Wake-on-Motion logic.
 *
 * @tessera expose category=tile name=wom_disable
 * @param  tile  Initialized tile handle
 */
void tile_sense_i_9_wom_disable(tile_t* tile);

/* ================================================================
 * FIFO
 * ================================================================ */

/**
 * @brief  Configure which sensor streams write into the FIFO.
 *
 * @tessera expose category=tile name=fifo_config
 *
 * Enables FIFO operation in USER_CTRL and selects the data sources.
 * Always sets FIFO_MODE = stream when enabling. Disables FIFO entirely
 * if accel, gyro and temp are all 0.
 *
 * @param  tile   Initialized tile handle
 * @param  mode   FIFO operating mode (stream or snapshot)
 * @param  accel  1 to write all 3 accel axes
 * @param  gyro   1 to write all 3 gyro axes
 * @param  temp   1 to write the temperature sample
 */
void tile_sense_i_9_fifo_config(tile_t* tile, sense_i_9_fifo_mode_t mode,
                                uint8_t accel, uint8_t gyro, uint8_t temp);

/**
 * @brief  Reset the FIFO contents (clears all queued samples).
 *
 * @tessera expose category=tile name=fifo_flush
 * @param  tile  Initialized tile handle
 */
void tile_sense_i_9_fifo_flush(tile_t* tile);

/**
 * @brief  Read the current FIFO byte count.
 *
 * @tessera expose category=tile name=fifo_count returns=int
 *
 * Note this is bytes, not packets. A standard accel+gyro packet is
 * 12 bytes. Read FIFO_COUNTL first to latch both bytes (handled
 * internally).
 *
 * @param  tile  Initialized tile handle
 * @return Bytes available in the FIFO (0–512)
 */
uint16_t tile_sense_i_9_fifo_count(tile_t* tile);

/**
 * @brief  Read one accel + gyro packet from the FIFO.
 *
 * @param  tile  Initialized tile handle
 * @param  pkt   Output packet (populated only if return is 1)
 * @return 1 if a 12-byte packet was read, 0 if FIFO has fewer bytes
 */
uint8_t tile_sense_i_9_fifo_read_packet(tile_t* tile,
                                        sense_i_9_fifo_packet_t* pkt);

/* ================================================================
 * Self-test
 * ================================================================ */

/**
 * @brief  Run the built-in mechanical self-test for accel and gyro.
 *
 * @tessera expose category=tile name=self_test returns=bool
 *
 * Drives the accel and gyro through their factory-stored
 * self-excitation routine and compares the response to the
 * stored reference values in SELF_TEST_*_GYRO/ACCEL (Bank 1).
 * Per TDK app-note AN-000150 the pass criterion is that each
 * axis's self-test response is within 50%–150% of the factory
 * reference.
 *
 * Blocks for ~250 ms. The chip is left in a fresh-init state on
 * exit; ranges/ODRs you set before this call may need to be
 * reapplied.
 *
 * @param  tile        Initialized tile handle
 * @param  accel_pass  Output: bit 0/1/2 = X/Y/Z accel pass (1 = pass)
 * @param  gyro_pass   Output: bit 0/1/2 = X/Y/Z gyro pass (1 = pass)
 * @return 1 if every accel axis and every gyro axis passed, 0 otherwise
 */
uint8_t tile_sense_i_9_self_test(tile_t* tile,
                                 uint8_t* accel_pass, uint8_t* gyro_pass);

/**
 * @brief  Run the AK09916 self-test (mag).
 *
 * @tessera expose category=tile name=mag_self_test returns=bool
 *
 * Triggers the AK09916's internal magnetic-source self-excitation.
 * The pass criterion is the per-axis range table from the AK09916
 * datasheet rev 015007392-E-02 §9.4.4.2:
 *   −200 ≤ HX ≤ 200,  −200 ≤ HY ≤ 200,  −1000 ≤ HZ ≤ −200
 *
 * Blocks for ~10 ms. Leaves the AK09916 in power-down — call
 * tile_sense_i_9_set_mag_mode() afterwards to resume measurements.
 *
 * @param  tile  Initialized tile handle
 * @return 1 if all three axes were within spec, 0 otherwise
 */
uint8_t tile_sense_i_9_mag_self_test(tile_t* tile);

#endif /* INC_TILE_SENSE_I_9_H_ */
