/**
 * @file   tile_drive_p.c
 * @brief  Piezoelectric haptic driver implementation (BOS1921).
 */

#include "tile_drive_p.h"
#include <stddef.h>

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

static const uint8_t id_table[] = {
    BOS1921_I2C_ADDR_DEFAULT,   /* instance 0 — fixed address (0x44) */
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

static void bos_write(tile_t* tile, uint8_t reg, uint16_t value)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFF);
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, buf, 2);
}

static uint16_t bos_read(tile_t* tile)
{
    uint8_t buf[2] = {0, 0};
    tile->hal->i2c_read(tile->hal->handle, tile->id, 0x00, buf, 2);
    return ((uint16_t)buf[0] << 8) | buf[1];
}

static void bos_set_return_reg(tile_t* tile, uint8_t reg)
{
    bos_write(tile, BOS1921_REG_COMM, (uint16_t)reg);
}

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

uint8_t tile_drive_p_find(tiles_pal_t* hal, uint8_t instance)
{
    uint8_t id = resolve_id(instance);
    if (id == 0x00) return 0;
    return (hal->i2c_is_ready(hal->handle, id) == 0) ? 1 : 0;
}

void tile_drive_p_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                       const drive_p_cfg_t *cfg)
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

    /* Wake the chip by writing to REFERENCE register */
    bos_write(tile, BOS1921_REG_REFERENCE, 0x0000);

    /* Software reset */
    bos_write(tile, BOS1921_REG_CONFIG, 0x0040);
    hal->delay_ms(1);

    /* Read chip ID (default return register after reset) */
    uint16_t chip_id = bos_read(tile);
    if ((chip_id & 0x0FFF) != BOS1921_CHIP_ID_DEFAULT) {
        TILE_ON_ERROR(tile, "init: unexpected chip ID");
        tile->state = TILE_STATE_ERROR;
        return;
    }

    /* Configure for 260nF piezo, L1=10µH, Rsense=0.2Ω, VDD=3.7V LiPo */
    bos_write(tile, BOS1921_REG_PARCAP,   0x043A);
    bos_write(tile, BOS1921_REG_SUP_RISE, 0x49E2);

    tile->state = TILE_STATE_READY;
}

void tile_drive_p_reset(tile_t* tile)
{
    bos_write(tile, BOS1921_REG_CONFIG, 0x0040);
    tile->state = TILE_STATE_NONE;
}

void tile_drive_p_set_mode(tile_t* tile, drive_p_mode_t mode)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "set_mode: not ready");
        return;
    }

    switch (mode) {
    default:
    case DRIVE_P_MODE_IDLE:
        bos_write(tile, BOS1921_REG_CONFIG, 0x0000);
        bos_set_return_reg(tile, BOS1921_REG_IC_STATUS);
        break;

    case DRIVE_P_MODE_SENSE_FINE:
        bos_write(tile, BOS1921_REG_CONFIG, 0x0010);
        bos_write(tile, BOS1921_REG_REFERENCE, 0x0000);
        tile->hal->delay_ms(5);
        bos_write(tile, BOS1921_REG_CONFIG, 0x0000);
        bos_write(tile, BOS1921_REG_CONFIG, 0x3010);
        bos_set_return_reg(tile, BOS1921_REG_SENSE_VAL);
        break;

    case DRIVE_P_MODE_SENSE_COARSE:
        bos_write(tile, BOS1921_REG_CONFIG, 0x0010);
        bos_write(tile, BOS1921_REG_REFERENCE, 0x0000);
        tile->hal->delay_ms(5);
        bos_write(tile, BOS1921_REG_CONFIG, 0x0000);
        bos_write(tile, BOS1921_REG_CONFIG, 0x2010);
        bos_set_return_reg(tile, BOS1921_REG_SENSE_VAL);
        break;

    case DRIVE_P_MODE_PLAY_DIRECT:
        bos_write(tile, BOS1921_REG_CONFIG, 0x0010);
        bos_set_return_reg(tile, BOS1921_REG_IC_STATUS);
        break;

    case DRIVE_P_MODE_PLAY_FIFO:
        bos_write(tile, BOS1921_REG_CONFIG, 0x0217);
        bos_set_return_reg(tile, BOS1921_REG_IC_STATUS);
        break;

    case DRIVE_P_MODE_PLAY_RAM_SYNTH:
        bos_write(tile, BOS1921_REG_CONFIG, 0x0610);
        bos_set_return_reg(tile, BOS1921_REG_IC_STATUS);
        break;
    }
}

uint16_t tile_drive_p_read(tile_t* tile)
{
    return bos_read(tile);
}

int16_t tile_drive_p_read_sense(tile_t* tile)
{
    bos_set_return_reg(tile, BOS1921_REG_SENSE_VAL);
    uint16_t raw = bos_read(tile);
    raw &= 0x0FFF;
    if (raw & 0x0800) {
        raw |= 0xF000;
    }
    return (int16_t)raw;
}

uint16_t tile_drive_p_read_status(tile_t* tile)
{
    bos_set_return_reg(tile, BOS1921_REG_IC_STATUS);
    return bos_read(tile);
}

void tile_drive_p_write_fifo(tile_t* tile, int16_t sample)
{
    bos_write(tile, BOS1921_REG_REFERENCE, (uint16_t)sample);
}

void tile_drive_p_write_reg(tile_t* tile, uint8_t reg, uint16_t value)
{
    bos_write(tile, reg, value);
}

void tile_drive_p_wfs_write(tile_t* tile, const uint16_t* words, uint16_t count)
{
    uint8_t buf[16];
    if (count > 8) count = 8;
    for (uint16_t i = 0; i < count; i++) {
        buf[i * 2]     = (uint8_t)(words[i] >> 8);
        buf[i * 2 + 1] = (uint8_t)(words[i] & 0xFF);
    }
    tile->hal->i2c_write(tile->hal->handle, tile->id,
                         BOS1921_REG_REFERENCE, buf, count * 2);
}

void tile_drive_p_sleep(tile_t* tile)
{
    bos_write(tile, BOS1921_REG_CONFIG, 0x0008);
    tile->state = TILE_STATE_SLEEPING;
}

uint8_t tile_drive_p_check_and_recover(tile_t* tile, drive_p_mode_t restore_mode)
{
    bos_set_return_reg(tile, BOS1921_REG_IC_STATUS);
    uint16_t status = bos_read(tile);

    uint8_t needs_recovery = 0;

    if ((status & BOS_STATUS_STATE_MASK) == BOS_STATUS_STATE_ERROR) {
        needs_recovery = 1;
    }
    if (status & BOS_STATUS_FAULT_MASK) {
        needs_recovery = 1;
    }

    if (needs_recovery) {
        tile_drive_p_reset(tile);
        tile->hal->delay_ms(1);
        tile->state = TILE_STATE_READY;  /* restore before set_mode */
        tile_drive_p_set_mode(tile, restore_mode);
    }

    return needs_recovery;
}
