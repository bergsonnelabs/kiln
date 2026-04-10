/**
 * @file   tile_disp_rgbw.c
 * @brief  Disp.RGBW (LP5811) — platform-agnostic driver.
 */

#include "tile_disp_rgbw.h"
#include <stddef.h>

/* ---- Instance table ---- */

static const uint8_t id_table[] = { LP5811_I2C_ADDR_DEFAULT };
#define NUM_INSTANCES  (sizeof(id_table) / sizeof(id_table[0]))

static uint8_t resolve_id(uint8_t instance)
{
    return (instance < NUM_INSTANCES) ? id_table[instance] : 0;
}

/* ---- Private helpers ---- */

static void lp_write(tile_t *tile, uint8_t reg, uint8_t val)
{
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, &val, 1);
}

static uint8_t lp_read(tile_t *tile, uint8_t reg)
{
    uint8_t val = 0;
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, &val, 1);
    return val;
}

/* ---- Public API ---- */

uint8_t tile_disp_rgbw_find(tiles_pal_t *hal, uint8_t instance)
{
    uint8_t addr = resolve_id(instance);
    if (!addr) return 0;
    return hal->i2c_is_ready(hal->handle, addr) == 0;
}

void tile_disp_rgbw_init(tiles_pal_t *hal, uint8_t instance, tile_t *tile)
{
    for (uint8_t i = 0; i < sizeof(tile_t); i++)
        ((uint8_t *)tile)[i] = 0;

    tile->hal = hal;
    tile->id  = resolve_id(instance);

    if (!tile->id) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "disp_rgbw: invalid instance");
        return;
    }

    if (hal->i2c_is_ready(hal->handle, tile->id) != 0) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "disp_rgbw: device not found");
        return;
    }

    /* Enable chip */
    lp_write(tile, LP5811_REG_CHIP_EN, 0x01);
    hal->delay_ms(2);

    /* Verify chip is alive */
    uint8_t cfg2 = lp_read(tile, LP5811_REG_CONFIG_2);
    if (cfg2 != LP5811_CONFIG_2_DEFAULT) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "disp_rgbw: CONFIG_2 verify failed");
        return;
    }

    /* Boost voltage 4.5V, max current 51mA */
    lp_write(tile, LP5811_REG_CONFIG_0, 0x1F);

    /* LOD + LSD protection */
    lp_write(tile, LP5811_REG_CONFIG_12, 0x0B);

    /* Commit config */
    lp_write(tile, LP5811_REG_CMD_UPDATE, 0x55);

    /* Enable all 4 LED channels */
    lp_write(tile, LP5811_REG_LED_EN, 0x0F);

    /* Default current limits: 50% */
    lp_write(tile, LP5811_REG_DC_0, 0x80);
    lp_write(tile, LP5811_REG_DC_1, 0x80);
    lp_write(tile, LP5811_REG_DC_2, 0x80);
    lp_write(tile, LP5811_REG_DC_3, 0x80);

    tile->state = TILE_STATE_READY;
}

void tile_disp_rgbw_set(tile_t *tile, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    /* Channel mapping: LED0=R, LED1=B, LED2=G, LED3=W */
    lp_write(tile, LP5811_REG_PWM_0, r);
    lp_write(tile, LP5811_REG_PWM_1, b);
    lp_write(tile, LP5811_REG_PWM_2, g);
    lp_write(tile, LP5811_REG_PWM_3, w);
}

void tile_disp_rgbw_off(tile_t *tile)
{
    lp_write(tile, LP5811_REG_PWM_0, 0);
    lp_write(tile, LP5811_REG_PWM_1, 0);
    lp_write(tile, LP5811_REG_PWM_2, 0);
    lp_write(tile, LP5811_REG_PWM_3, 0);
}

void tile_disp_rgbw_set_current(tile_t *tile, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    lp_write(tile, LP5811_REG_DC_0, r);
    lp_write(tile, LP5811_REG_DC_1, b);
    lp_write(tile, LP5811_REG_DC_2, g);
    lp_write(tile, LP5811_REG_DC_3, w);
}

void tile_disp_rgbw_sleep(tile_t *tile)
{
    lp_write(tile, LP5811_REG_CHIP_EN, 0x00);
    tile->state = TILE_STATE_SLEEPING;
}

void tile_disp_rgbw_wake(tile_t *tile)
{
    lp_write(tile, LP5811_REG_CHIP_EN, 0x01);
    tile->hal->delay_ms(2);
    tile->state = TILE_STATE_READY;
}

void tile_disp_rgbw_reset(tile_t *tile)
{
    lp_write(tile, LP5811_REG_RESET, 0xFF);
    tile->hal->delay_ms(2);
    tile->state = TILE_STATE_NONE;
}
