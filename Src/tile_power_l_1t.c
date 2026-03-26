/**
 * @file   tile_power_l_1t.c
 * @brief  Li-Ion charge controller implementation (BQ25150).
 */

#include "tile_power_l_1t.h"

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

static const uint8_t id_table[] = {
    BQ25150_I2C_ADDR_DEFAULT,   /* instance 0 — fixed address (0x6B) */
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

static void bq_write(tile_t* tile, uint8_t reg, uint8_t value)
{
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, &value, 1);
}

static uint8_t bq_read(tile_t* tile, uint8_t reg)
{
    uint8_t value = 0;
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, &value, 1);
    return value;
}

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

uint8_t tile_power_l_1t_find(tiles_hal_t* hal, uint8_t instance)
{
    uint8_t id = resolve_id(instance);
    if (id == 0x00) return 0;
    return (hal->i2c_is_ready(hal->handle, id) == 0) ? 1 : 0;
}

void tile_power_l_1t_init(tiles_hal_t* hal, uint8_t instance, tile_t* tile)
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

    /* Verify device ID */
    uint8_t dev_id = bq_read(tile, BQ25150_REG_DEVICE_ID);
    if (dev_id != BQ25150_DEVICE_ID_DEFAULT) {
        TILE_ON_ERROR(tile, "init: unexpected device ID");
        tile->state = TILE_STATE_ERROR;
        return;
    }

    /* Fast charge current: 80 mA */
    bq_write(tile, BQ25150_REG_ICHG_CTRL, 0x40);

    /* Precharge current: 20 mA */
    bq_write(tile, BQ25150_REG_PCHRGCTRL, 0x0F);

    /* Battery undervoltage lockout: 2.6 V */
    bq_write(tile, BQ25150_REG_BUVLO, 0x04);

    /* Enable VBAT ADC channel */
    bq_write(tile, BQ25150_REG_ADC_READ_EN, 0x08);

    /* ADC: 1-second update rate in battery mode */
    bq_write(tile, BQ25150_REG_ADCCTRL0, 0x82);

    /* Disable ship mode */
    bq_write(tile, BQ25150_REG_ICCTRL0, 0x00);

    tile->state = TILE_STATE_READY;
}

uint16_t tile_power_l_1t_get_vbat(tile_t* tile)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "get_vbat: not ready");
        return 0;
    }

    uint8_t msb = 0, lsb = 0;
    int rc = tile->hal->i2c_read(tile->hal->handle, tile->id,
                                  BQ25150_REG_VBAT_MSB, &msb, 1);
    if (rc != 0) return 0;
    tile->hal->i2c_read(tile->hal->handle, tile->id,
                         BQ25150_REG_VBAT_LSB, &lsb, 1);
    return ((uint16_t)msb << 8) | lsb;
}

uint8_t tile_power_l_1t_get_percent(tile_t* tile)
{
    uint8_t result = 0;
    uint16_t voltage = tile_power_l_1t_get_vbat(tile);
    if (voltage < 38229) {
        result = 0;
    } else {
        result = (uint8_t)((voltage / 76.458) - 500);
    }
    if (result > 100) {
        result = 100;
    }
    return result;
}

uint8_t tile_power_l_1t_read_status(tile_t* tile, uint8_t reg)
{
    return bq_read(tile, reg);
}

void tile_power_l_1t_write_reg(tile_t* tile, uint8_t reg, uint8_t value)
{
    bq_write(tile, reg, value);
}
