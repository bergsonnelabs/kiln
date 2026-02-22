#ifndef INC_TILE_SENSE_I_9_H_
#define INC_TILE_SENSE_I_9_H_

#include "main.h"
#include "stdint.h"

#define ICM20948_I2C_ADDR			0x69

#define ICM20948_BANK_0             0x00
#define ICM20948_BANK_1             0x01
#define ICM20948_BANK_2             0x02
#define ICM20948_BANK_3             0x03
#define ICM20948_BANK_4             0x04
#define ICM20948_REG_BANK_SEL       0x7F

// BANK 0 REGISTERS
#define ICM20948_REG_WHOAMI         0x00
#define ICM20948_USER_CTRL          0x03
#define ICM20948_REG_PWR_MGMT_1     0x06
#define ICM20948_REG_PWR_MGMT_2     0x07
#define ICM20948_REG_INT_PIN_CFG    0x0F
#define ICM20948_REG_ACCEL_X_H      0X2D
#define ICM20948_REG_ACCEL_X_L      0X2E
#define ICM20948_REG_ACCEL_Y_H      0X2F
#define ICM20948_REG_ACCEL_Y_L      0X30
#define ICM20948_REG_ACCEL_Z_H      0X31
#define ICM20948_REG_ACCEL_Z_L      0X32
#define ICM20948_REG_GYRO_X_H       0X33
#define ICM20948_REG_GYRO_X_L       0X34
#define ICM20948_REG_GYRO_Y_H       0X35
#define ICM20948_REG_GYRO_Y_L       0X36
#define ICM20948_REG_GYRO_Z_H       0X37
#define ICM20948_REG_GYRO_Z_L       0X38
#define ICM20948_REG_TEMP_H         0X39
#define ICM20948_REG_TEMP_L         0X3A

// BANK 2 REGISTERS
#define ICM20948_REG_GYRO_CONFIG    0x01
#define ICM20948_REG_ACCEL_CONFIG   0x14

// MAGNETOMETER REGISTERS
#define AK09916_I2C_ADDR            0x0C

#define AK09916_REG_WIA2            0x01
#define AK09916_REG_CNTL2           0x31
#define AK09916_REG_HXL             0x11
#define AK09916_REG_HXH             0x12
#define AK09916_REG_ST2             0x18

// VALUES
#define ICM20948_REG_WHOAMI_DEFAULT  0xEA

typedef enum
{
    ACCEL_2G   = (0x00),
    ACCEL_4G   = (0x01),
    ACCEL_8G   = (0x02),
    ACCEL_16G  = (0x03),
} a_range_t;

typedef enum
{
    GYRO_250DPS  = (0x00),
    GYRO_500DPS  = (0x01),
    GYRO_1000DPS = (0x02),
    GYRO_2000DPS = (0x03),
} g_range_t;

uint8_t tile_sense_i_9_find(I2C_HandleTypeDef* hi2c);
uint8_t tile_sense_i_9_init(I2C_HandleTypeDef* hi2c);
void tile_sense_i_9_set_accel_range(a_range_t accel_range);
void tile_sense_i_9_set_gyro_range(g_range_t gyro_range);

// ----------------------------------------------------------
// GET RAW ACCEL + GYRO DATA
// pass a 6-element int16_t buffer, which will be filled:
// [AXH-AXL AYH-AYL AZH-AZL GXH-GXL GYH-GYL GZH-GZL]
// returns 1 if 12 bytes read, otherwise 0
void tile_sense_i_9_get_raw_6dof(int16_t* buffer);

// ----------------------------------------------------------
// GET RAW ACCEL DATA
// pass a 3-element int16_t buffer, which will be filled:
// [AXH-AXL AYH-AYL AZH-AZL]
// returns 1 if 6 bytes read, otherwise 0
void tile_sense_i_9_get_raw_accels(int16_t* buffer);

// ----------------------------------------------------------
// GET RAW MAGNETOMETER DATA
// pass a 3-element int16_t buffer, which will be filled:
// [HXH-HXL HYH-HYL HZH-HZL]
// returns 1 if 6 bytes read, otherwise 0
void tile_sense_i_9_get_raw_mags(int16_t* buffer);


#endif
