/**
 * @file   tile_sense_i_9.c
 * @brief  9-DOF IMU driver implementation (ICM-20948 + AK09916).
 */

#include "tile_sense_i_9.h"

/* -------------------------------------------------------------- */
/* Private state                                                   */
/* -------------------------------------------------------------- */

static kiln_hal_t* hal_ptr = 0;
static uint8_t icm_addr = ICM20948_I2C_ADDR_DEFAULT;

/* -------------------------------------------------------------- */
/* Private helpers                                                 */
/* -------------------------------------------------------------- */

static void icm_write(uint8_t reg, uint8_t value)
{
    hal_ptr->i2c_write(hal_ptr->handle, icm_addr, reg, &value, 1);
}

static void icm_read(uint8_t reg, uint8_t* data, uint16_t len)
{
    hal_ptr->i2c_read(hal_ptr->handle, icm_addr, reg, data, len);
}

static void ak_write(uint8_t reg, uint8_t value)
{
    hal_ptr->i2c_write(hal_ptr->handle, AK09916_I2C_ADDR, reg, &value, 1);
}

static void ak_read(uint8_t reg, uint8_t* data, uint16_t len)
{
    hal_ptr->i2c_read(hal_ptr->handle, AK09916_I2C_ADDR, reg, data, len);
}

static void set_bank(uint8_t bank)
{
    icm_write(ICM20948_REG_BANK_SEL, bank << 4);
}

/** @brief  Swap bytes in a buffer of int16_t values (big-endian → native). */
static void swap16(int16_t* buf, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        buf[i] = (int16_t)__builtin_bswap16((uint16_t)buf[i]);
    }
}

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

uint8_t tile_sense_i_9_find(kiln_hal_t* hal, uint8_t addr)
{
    return (hal->i2c_is_ready(hal->handle, addr) == 0) ? 1 : 0;
}

uint8_t tile_sense_i_9_init(kiln_hal_t* hal, uint8_t addr)
{
    uint8_t whoami = 0;

    hal_ptr = hal;
    icm_addr = addr;

    /* Verify chip identity */
    set_bank(ICM20948_BANK_0);
    icm_read(ICM20948_REG_WHOAMI, &whoami, 1);
    if (whoami != ICM20948_WHOAMI_DEFAULT) {
        return 0;
    }

    /* Software reset */
    icm_write(ICM20948_REG_PWR_MGMT_1, 1 << 7);
    hal_ptr->delay_ms(50);

    /* Wake: auto clock select */
    icm_write(ICM20948_REG_PWR_MGMT_1, 0x01);

    /* Enable all accel + gyro axes */
    icm_write(ICM20948_REG_PWR_MGMT_2, 0x00);

    /* Enable I2C bypass so we can talk to the AK09916 directly */
    icm_write(ICM20948_REG_INT_PIN_CFG, 0x02);

    /* Return to bank 0 */
    set_bank(ICM20948_BANK_0);

    /* Initialize magnetometer: continuous 100 Hz */
    ak_write(AK09916_REG_CNTL3, 0x01);  /* soft reset */
    hal_ptr->delay_ms(1);
    ak_write(AK09916_REG_CNTL2, MAG_CONTINUOUS_100HZ);

    return 1;
}

void tile_sense_i_9_set_accel_range(accel_range_t range)
{
    set_bank(ICM20948_BANK_2);
    icm_write(ICM20948_REG_ACCEL_CONFIG, (uint8_t)range);
    set_bank(ICM20948_BANK_0);
}

void tile_sense_i_9_set_gyro_range(gyro_range_t range)
{
    set_bank(ICM20948_BANK_2);
    icm_write(ICM20948_REG_GYRO_CONFIG, (uint8_t)range);
    set_bank(ICM20948_BANK_0);
}

void tile_sense_i_9_set_mag_mode(mag_mode_t mode)
{
    ak_write(AK09916_REG_CNTL2, (uint8_t)mode);
}

void tile_sense_i_9_set_accel_odr(uint16_t divider)
{
    uint8_t hi = (uint8_t)((divider >> 8) & 0x0F);
    uint8_t lo = (uint8_t)(divider & 0xFF);

    set_bank(ICM20948_BANK_2);
    icm_write(ICM20948_REG_ACCEL_SMPLRT_H, hi);
    icm_write(ICM20948_REG_ACCEL_SMPLRT_L, lo);
    set_bank(ICM20948_BANK_0);
}

void tile_sense_i_9_set_gyro_odr(uint8_t divider)
{
    set_bank(ICM20948_BANK_2);
    icm_write(ICM20948_REG_GYRO_SMPLRT, divider);
    set_bank(ICM20948_BANK_0);
}

void tile_sense_i_9_get_raw_accels(int16_t* buffer)
{
    icm_read(ICM20948_REG_ACCEL_X_H, (uint8_t*)buffer, 6);
    swap16(buffer, 3);
}

void tile_sense_i_9_get_raw_gyros(int16_t* buffer)
{
    icm_read(ICM20948_REG_GYRO_X_H, (uint8_t*)buffer, 6);
    swap16(buffer, 3);
}

void tile_sense_i_9_get_raw_6dof(int16_t* buffer)
{
    icm_read(ICM20948_REG_ACCEL_X_H, (uint8_t*)buffer, 12);
    swap16(buffer, 6);
}

void tile_sense_i_9_get_raw_mags(int16_t* buffer)
{
    uint8_t st2;

    /* AK09916 data registers are little-endian — no swap needed */
    ak_read(AK09916_REG_HXL, (uint8_t*)buffer, 6);

    /* Reading ST2 releases the data lock for the next measurement */
    ak_read(AK09916_REG_ST2, &st2, 1);
}

int16_t tile_sense_i_9_get_temperature(void)
{
    int16_t raw;
    icm_read(ICM20948_REG_TEMP_H, (uint8_t*)&raw, 2);
    raw = (int16_t)__builtin_bswap16((uint16_t)raw);
    return raw;
}

void tile_sense_i_9_sleep(void)
{
    set_bank(ICM20948_BANK_0);
    icm_write(ICM20948_REG_PWR_MGMT_1, 0x41);  /* SLEEP + auto clock */
}

void tile_sense_i_9_wake(void)
{
    set_bank(ICM20948_BANK_0);
    icm_write(ICM20948_REG_PWR_MGMT_1, 0x01);  /* clear SLEEP, auto clock */
}

void tile_sense_i_9_reset(void)
{
    set_bank(ICM20948_BANK_0);
    icm_write(ICM20948_REG_PWR_MGMT_1, 1 << 7);
    hal_ptr->delay_ms(50);
}
