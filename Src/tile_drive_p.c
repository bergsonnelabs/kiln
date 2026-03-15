/**
 * @file   tile_drive_p.c
 * @brief  Piezoelectric haptic driver implementation (BOS1921).
 */

#include "tile_drive_p.h"

/* -------------------------------------------------------------- */
/* Private state                                                   */
/* -------------------------------------------------------------- */

static kiln_hal_t* hal_ptr = 0;
static uint8_t bos_addr = BOS1921_I2C_ADDR_DEFAULT;

/* -------------------------------------------------------------- */
/* Private helpers                                                 */
/* -------------------------------------------------------------- */

/**
 * @brief  Write a 16-bit value to a BOS1921 register (big-endian).
 */
static void bos_write(uint8_t reg, uint16_t value)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFF);
    hal_ptr->i2c_write(hal_ptr->handle, bos_addr, reg, buf, 2);
}

/**
 * @brief  Read a 16-bit value from the BOS1921 default return register.
 *
 * The BOS1921 always returns data from the register selected via
 * BOS1921_REG_COMM, read from register address 0x00.
 */
static uint16_t bos_read(void)
{
    uint8_t buf[2] = {0, 0};
    hal_ptr->i2c_read(hal_ptr->handle, bos_addr, 0x00, buf, 2);
    return ((uint16_t)buf[0] << 8) | buf[1];
}

/**
 * @brief  Set the return register for subsequent reads.
 */
static void bos_set_return_reg(uint8_t reg)
{
    bos_write(BOS1921_REG_COMM, (uint16_t)reg);
}

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

uint8_t tile_drive_p_find(kiln_hal_t* hal, uint8_t addr)
{
    return (hal->i2c_is_ready(hal->handle, addr) == 0) ? 1 : 0;
}

uint8_t tile_drive_p_init(kiln_hal_t* hal, uint8_t addr)
{
    uint16_t chip_id;

    hal_ptr = hal;
    bos_addr = addr;

    /* Wake the chip by writing to REFERENCE register */
    bos_write(BOS1921_REG_REFERENCE, 0x0000);

    /* Software reset */
    tile_drive_p_reset();

    /* Read chip ID (default return register after reset) */
    chip_id = bos_read();
    if ((chip_id & 0x0FFF) != BOS1921_CHIP_ID_DEFAULT) {
        return 0;
    }

    return 1;
}

void tile_drive_p_select(kiln_hal_t* hal, uint8_t addr)
{
    hal_ptr = hal;
    bos_addr = addr;
}

void tile_drive_p_reset(void)
{
    bos_write(BOS1921_REG_CONFIG, 0x0040);  /* RST bit */
}

void tile_drive_p_set_mode(drive_p_mode_t mode)
{
    switch (mode) {
    default:
    case DRIVE_P_MODE_IDLE:
        bos_write(BOS1921_REG_CONFIG, 0x0000);
        bos_set_return_reg(BOS1921_REG_IC_STATUS);
        break;

    case DRIVE_P_MODE_SENSE_FINE:
        /* SENSE=1, GAINS=1 (7.6 mV/LSB), OE=1 */
        bos_write(BOS1921_REG_CONFIG, 0x3010);
        bos_set_return_reg(BOS1921_REG_SENSE_VAL);
        break;

    case DRIVE_P_MODE_SENSE_COARSE:
        /* SENSE=1, GAINS=0 (54.5 mV/LSB), OE=1 */
        bos_write(BOS1921_REG_CONFIG, 0x2010);
        bos_set_return_reg(BOS1921_REG_SENSE_VAL);
        break;

    case DRIVE_P_MODE_PLAY_DIRECT:
        /* OE=1, PLAY_MODE=0 (direct) */
        bos_write(BOS1921_REG_CONFIG, 0x0010);
        bos_set_return_reg(BOS1921_REG_IC_STATUS);
        break;

    case DRIVE_P_MODE_PLAY_FIFO:
        /* OE=1, PLAY_MODE=1 (FIFO), PLAY_SRATE=7 (8 ksps) */
        bos_write(BOS1921_REG_CONFIG, 0x0217);
        bos_set_return_reg(BOS1921_REG_IC_STATUS);
        break;
    }
}

uint16_t tile_drive_p_read(void)
{
    return bos_read();
}

int16_t tile_drive_p_read_sense(void)
{
    bos_set_return_reg(BOS1921_REG_SENSE_VAL);
    uint16_t raw = bos_read();
    /* Bits [15:12] are flags (POL_SENS, SENSE, GAIN, SENS_FLAG).
     * Bits [11:0] are the 12-bit signed sense value. Mask then sign-extend. */
    raw &= 0x0FFF;
    if (raw & 0x0800) {
        raw |= 0xF000;
    }
    return (int16_t)raw;
}

uint16_t tile_drive_p_read_status(void)
{
    bos_set_return_reg(BOS1921_REG_IC_STATUS);
    return bos_read();
}

void tile_drive_p_write_fifo(int16_t sample)
{
    bos_write(BOS1921_REG_REFERENCE, (uint16_t)sample);
}

void tile_drive_p_write_reg(uint8_t reg, uint16_t value)
{
    bos_write(reg, value);
}

void tile_drive_p_sleep(void)
{
    /* DS=1 (deep sleep), everything else off */
    bos_write(BOS1921_REG_CONFIG, 0x0008);
}
