/**
 * @file   tile_drive_h.c
 * @brief  LRA haptic driver implementation (DRV2605L).
 */

#include "tile_drive_h.h"
#include <stddef.h>

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

static const uint8_t id_table[] = {
    DRV2605L_I2C_ADDR_DEFAULT,   /* instance 0 — fixed address (0x5A) */
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

static void drv_write(tile_t* tile, uint8_t reg, uint8_t value)
{
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, &value, 1);
}

static uint8_t drv_read(tile_t* tile, uint8_t reg)
{
    uint8_t val = 0;
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, &val, 1);
    return val;
}

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

uint8_t tile_drive_h_find(tiles_pal_t* hal, uint8_t instance)
{
    uint8_t id = resolve_id(instance);
    if (id == 0x00) return 0;
    return (hal->i2c_is_ready(hal->handle, id) == 0) ? 1 : 0;
}

void tile_drive_h_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile)
{
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

    /* Verify status register */
    uint8_t status = drv_read(tile, DRV2605L_REG_STATUS);
    if (status != DRV2605L_STATUS_DEFAULT) {
        TILE_ON_ERROR(tile, "init: unexpected status register");
        tile->state = TILE_STATE_ERROR;
        return;
    }

    /* Exit standby */
    drv_write(tile, DRV2605L_REG_MODE, 0x00);
    hal->delay_ms(400);

    /* LRA drive levels for VG0832013D (1.8Vrms rated, ~235Hz) */
    drv_write(tile, DRV2605L_REG_RATED_VOLTAGE, 0x56);
    drv_write(tile, DRV2605L_REG_OD_CLAMP, 0x8C);

    /* Configure for LRA: FEEDBACK_CTRL = 0xB6 */
    drv_write(tile, DRV2605L_REG_FEEDBACK_CTRL, 0xB6);
    hal->delay_ms(100);

    /* CONTROL3: LRA open loop */
    drv_write(tile, DRV2605L_REG_CONTROL3, 0x01);

    /* Select LRA waveform library */
    drv_write(tile, DRV2605L_REG_LIBRARY_SEL, 6);

    tile->state = TILE_STATE_READY;
}

void tile_drive_h_play(tile_t* tile, uint8_t index, uint8_t repeats)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "play: not ready");
        return;
    }

    /* Load effect into sequence slot 0, terminate in slot 1 */
    drv_write(tile, DRV2605L_REG_WAVE_SEQ_0, index);
    drv_write(tile, DRV2605L_REG_WAVE_SEQ_1, 0);

    /* Trigger */
    drv_write(tile, DRV2605L_REG_GO, 0x01);

    /* Repeat with gap */
    for (uint8_t i = 1; i < repeats; i++) {
        tile->hal->delay_ms(200);
        drv_write(tile, DRV2605L_REG_GO, 0x01);
    }
}

void tile_drive_h_stop(tile_t* tile)
{
    drv_write(tile, DRV2605L_REG_GO, 0x00);
}

void tile_drive_h_rtp_start(tile_t* tile)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "rtp_start: not ready");
        return;
    }
    drv_write(tile, DRV2605L_REG_MODE, 0x05);
    drv_write(tile, DRV2605L_REG_RTP, 0x00);
}

void tile_drive_h_rtp_write(tile_t* tile, uint8_t amplitude)
{
    drv_write(tile, DRV2605L_REG_RTP, amplitude);
}

void tile_drive_h_rtp_stop(tile_t* tile)
{
    drv_write(tile, DRV2605L_REG_RTP, 0x00);
    drv_write(tile, DRV2605L_REG_MODE, 0x00);
}
