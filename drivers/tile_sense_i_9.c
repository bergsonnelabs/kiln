/**
 * @file   tile_sense_i_9.c
 * @brief  9-DOF IMU driver implementation (ICM-20948 + AK09916).
 */

#include "tile_sense_i_9.h"
#include <stddef.h>

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

static const uint8_t id_table[] = {
    ICM20948_I2C_ADDR_DEFAULT,   /* instance 0 — pad 2 floating (0x69) */
    ICM20948_I2C_ADDR_ALT,       /* instance 1 — pad 2 to GND  (0x68) */
};

#define ID_TABLE_LEN  (sizeof(id_table) / sizeof(id_table[0]))

static uint8_t resolve_id(uint8_t instance)
{
    if (instance >= ID_TABLE_LEN) return 0x00;
    return id_table[instance];
}

/* -------------------------------------------------------------- */
/* Private helpers                                                 */
/* -------------------------------------------------------------- */

static void icm_write(tile_t* tile, uint8_t reg, uint8_t value)
{
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, &value, 1);
}

static void icm_read(tile_t* tile, uint8_t reg, uint8_t* data, uint16_t len)
{
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, data, len);
}

static void ak_write(tile_t* tile, uint8_t reg, uint8_t value)
{
    tile->hal->i2c_write(tile->hal->handle, AK09916_I2C_ADDR, reg, &value, 1);
}

static void ak_read(tile_t* tile, uint8_t reg, uint8_t* data, uint16_t len)
{
    tile->hal->i2c_read(tile->hal->handle, AK09916_I2C_ADDR, reg, data, len);
}

static void set_bank(tile_t* tile, uint8_t bank)
{
    icm_write(tile, ICM20948_REG_BANK_SEL, bank << 4);
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

uint8_t tile_sense_i_9_find(tiles_pal_t* hal, uint8_t instance)
{
    uint8_t id = resolve_id(instance);
    if (id == 0x00) return 0;
    return (hal->i2c_is_ready(hal->handle, id) == 0) ? 1 : 0;
}

void tile_sense_i_9_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                         const sense_i_9_cfg_t *cfg)
{
    (void)cfg;  /* Reserved for future use */
    tile->hal      = NULL;
    tile->id       = 0;
    tile->state    = TILE_STATE_NONE;
    tile->flags    = 0;
    tile->callback = NULL;
    tile->cb_ctx   = NULL;

    uint8_t id = resolve_id(instance);
    if (id == 0x00) {
        TILE_ON_ERROR(tile, "init: invalid instance");
        tile->state = TILE_STATE_ERROR;
        return;
    }

    tile->hal = hal;
    tile->id  = id;

    /* Verify device is on bus */
    if (hal->i2c_is_ready(hal->handle, id) != 0) {
        TILE_ON_ERROR(tile, "init: device not found on bus");
        tile->state = TILE_STATE_ERROR;
        return;
    }

    /* Verify chip identity */
    uint8_t whoami = 0;
    set_bank(tile, ICM20948_BANK_0);
    icm_read(tile, ICM20948_REG_WHOAMI, &whoami, 1);
    if (whoami != ICM20948_WHOAMI_DEFAULT) {
        TILE_ON_ERROR(tile, "init: unexpected chip ID");
        tile->state = TILE_STATE_ERROR;
        return;
    }

    /* Software reset */
    icm_write(tile, ICM20948_REG_PWR_MGMT_1, 1 << 7);
    hal->delay_ms(50);

    /* Wake: auto clock select */
    icm_write(tile, ICM20948_REG_PWR_MGMT_1, 0x01);

    /* Enable all accel + gyro axes */
    icm_write(tile, ICM20948_REG_PWR_MGMT_2, 0x00);

    /* Enable I2C bypass so we can talk to the AK09916 directly */
    icm_write(tile, ICM20948_REG_INT_PIN_CFG, 0x02);

    /* Return to bank 0 */
    set_bank(tile, ICM20948_BANK_0);

    /* Initialize magnetometer: continuous 100 Hz */
    ak_write(tile, AK09916_REG_CNTL3, 0x01);  /* soft reset */
    hal->delay_ms(1);
    ak_write(tile, AK09916_REG_CNTL2, SENSE_I_9_MAG_CONTINUOUS_100HZ);

    tile->state = TILE_STATE_READY;
}

uint8_t tile_sense_i_9_data_ready(tile_t* tile)
{
    uint8_t status = 0;
    icm_read(tile, ICM20948_REG_INT_STATUS_1, &status, 1);
    return (status & 0x01) ? 1 : 0;  /* RAW_DATA_0_RDY_INT */
}

void tile_sense_i_9_set_accel_range(tile_t* tile, sense_i_9_accel_range_t range)
{
    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_ACCEL_CONFIG, (uint8_t)range);
    set_bank(tile, ICM20948_BANK_0);
}

void tile_sense_i_9_set_gyro_range(tile_t* tile, sense_i_9_gyro_range_t range)
{
    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_GYRO_CONFIG, (uint8_t)range);
    set_bank(tile, ICM20948_BANK_0);
}

void tile_sense_i_9_set_mag_mode(tile_t* tile, sense_i_9_mag_mode_t mode)
{
    ak_write(tile, AK09916_REG_CNTL2, (uint8_t)mode);
}

void tile_sense_i_9_set_accel_odr(tile_t* tile, uint16_t divider)
{
    uint8_t hi = (uint8_t)((divider >> 8) & 0x0F);
    uint8_t lo = (uint8_t)(divider & 0xFF);

    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_ACCEL_SMPLRT_H, hi);
    icm_write(tile, ICM20948_REG_ACCEL_SMPLRT_L, lo);
    set_bank(tile, ICM20948_BANK_0);
}

void tile_sense_i_9_set_gyro_odr(tile_t* tile, uint8_t divider)
{
    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_GYRO_SMPLRT, divider);
    set_bank(tile, ICM20948_BANK_0);
}

void tile_sense_i_9_get_raw_accels(tile_t* tile, int16_t* buffer)
{
    icm_read(tile, ICM20948_REG_ACCEL_X_H, (uint8_t*)buffer, 6);
    swap16(buffer, 3);
}

void tile_sense_i_9_get_raw_gyros(tile_t* tile, int16_t* buffer)
{
    icm_read(tile, ICM20948_REG_GYRO_X_H, (uint8_t*)buffer, 6);
    swap16(buffer, 3);
}

void tile_sense_i_9_get_raw_6dof(tile_t* tile, int16_t* buffer)
{
    icm_read(tile, ICM20948_REG_ACCEL_X_H, (uint8_t*)buffer, 12);
    swap16(buffer, 6);
}

void tile_sense_i_9_get_raw_mags(tile_t* tile, int16_t* buffer)
{
    uint8_t st2;

    /* AK09916 data registers are little-endian — no swap needed */
    ak_read(tile, AK09916_REG_HXL, (uint8_t*)buffer, 6);

    /* Reading ST2 releases the data lock for the next measurement */
    ak_read(tile, AK09916_REG_ST2, &st2, 1);
}

int16_t tile_sense_i_9_get_temperature(tile_t* tile)
{
    int16_t raw;
    icm_read(tile, ICM20948_REG_TEMP_H, (uint8_t*)&raw, 2);
    raw = (int16_t)__builtin_bswap16((uint16_t)raw);
    return raw;
}

void tile_sense_i_9_sleep(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_0);
    icm_write(tile, ICM20948_REG_PWR_MGMT_1, 0x41);  /* SLEEP + auto clock */
    tile->state = TILE_STATE_SLEEPING;
}

void tile_sense_i_9_wake(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_0);
    icm_write(tile, ICM20948_REG_PWR_MGMT_1, 0x01);  /* clear SLEEP, auto clock */
    tile->state = TILE_STATE_READY;
}

void tile_sense_i_9_reset(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_0);
    icm_write(tile, ICM20948_REG_PWR_MGMT_1, 1 << 7);
    tile->hal->delay_ms(50);
    tile->state = TILE_STATE_NONE;
}
