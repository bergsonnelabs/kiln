/**
 * @file   tile_disp_rgbw.h
 * @brief  RGBW LED driver for the Display.RGBW tile (LP5811).
 * @version 1.0.0
 *
 * 4-channel LED driver with independent PWM + current control.
 * Channels: R (LED0), G (LED2), B (LED1), W (LED3).
 *
 * Quick start:
 * @code
 *   tile_t led;
 *   tile_disp_rgbw_init(&hal, 0, &led, NULL);
 *   tile_disp_rgbw_set(&led, 255, 0, 0, 0);   // red
 *   tile_disp_rgbw_set(&led, 0, 0, 0, 128);   // dim white
 *   tile_disp_rgbw_off(&led);                  // all off
 * @endcode
 *
 * @tessera tile label=Display.RGBW icon=◑
 */

#ifndef INC_TILE_DISP_RGBW_H_
#define INC_TILE_DISP_RGBW_H_

#include "tiles.h"
#include <stdint.h>

/* ---- Driver version ---- */

#define TILE_DISP_RGBW_VERSION_MAJOR  1
#define TILE_DISP_RGBW_VERSION_MINOR  0
#define TILE_DISP_RGBW_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);

/* ---- Instance mapping ---- */

/**
 * | Instance | ID   | Hardware config    |
 * |----------|------|--------------------|
 * | 0        | 0x50 | Default address    |
 */
#define LP5811_I2C_ADDR_DEFAULT  0x50

/* ---- LP5811 registers ---- */

#define LP5811_REG_CHIP_EN      0x00
#define LP5811_REG_CONFIG_0     0x01
#define LP5811_REG_CONFIG_2     0x03
#define LP5811_REG_CONFIG_12    0x0D
#define LP5811_REG_CMD_UPDATE   0x10
#define LP5811_REG_LED_EN       0x20
#define LP5811_REG_RESET        0x23
#define LP5811_REG_DC_0         0x30  /* Current limit channel 0 (R) */
#define LP5811_REG_DC_1         0x31  /* Current limit channel 1 (B) */
#define LP5811_REG_DC_2         0x32  /* Current limit channel 2 (G) */
#define LP5811_REG_DC_3         0x33  /* Current limit channel 3 (W) */
#define LP5811_REG_PWM_0        0x40  /* PWM channel 0 (R) */
#define LP5811_REG_PWM_1        0x41  /* PWM channel 1 (B) */
#define LP5811_REG_PWM_2        0x42  /* PWM channel 2 (G) */
#define LP5811_REG_PWM_3        0x43  /* PWM channel 3 (W) */

#define LP5811_CONFIG_2_DEFAULT 0xE4  /* Used to verify chip is alive */

/* ---- Public API ---- */

/** @brief  Check if a Disp.RGBW is present on the bus. */
uint8_t tile_disp_rgbw_find(tiles_pal_t *hal, uint8_t instance);

/**
 * Optional init config. Pass NULL for defaults.
 * Reserved for future use (e.g., initial brightness, current limits).
 */
typedef struct {
    uint8_t reserved;   /**< Placeholder — no options yet. */
} disp_rgbw_cfg_t;

/**
 * @brief  Initialize the LP5811 LED driver.
 *
 * Enables the chip, configures boost voltage to 4.5V, sets max
 * current to 51mA, enables all 4 LED channels, and sets current
 * limits to 50%. Pass cfg=NULL for defaults.
 *
 * @param  hal       Platform abstraction handle
 * @param  instance  Device instance (0 = default address 0x50)
 * @param  tile      Tile handle to populate
 * @param  cfg       Optional config, or NULL for defaults
 */
void tile_disp_rgbw_init(tiles_pal_t *hal, uint8_t instance, tile_t *tile,
                         const disp_rgbw_cfg_t *cfg);

/**
 * @brief Set RGBW output levels.
 *
 * @tessera expose category=tile icon=◑ name=set
 * @param r [0..255] Red PWM.
 * @param g [0..255] Green PWM.
 * @param b [0..255] Blue PWM.
 * @param w [0..255] White PWM.
 */
void tile_disp_rgbw_set(tile_t *tile, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/**
 * @brief Turn all LEDs off (PWM = 0).
 *
 * @tessera expose category=tile icon=◑ name=off
 */
void tile_disp_rgbw_off(tile_t *tile);

/**
 * @brief Set per-channel current limit.
 *
 * @tessera expose category=tile icon=◑ name=set_current
 * @param r [0..255] Red current (fraction of 51 mA max).
 * @param g [0..255] Green current.
 * @param b [0..255] Blue current.
 * @param w [0..255] White current.
 */
void tile_disp_rgbw_set_current(tile_t *tile, uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/**
 * @brief Enter sleep (disable chip).
 *
 * @tessera expose category=tile icon=◑ name=sleep
 */
void tile_disp_rgbw_sleep(tile_t *tile);

/**
 * @brief Wake (re-enable chip, LEDs retain previous state).
 *
 * @tessera expose category=tile icon=◑ name=wake
 */
void tile_disp_rgbw_wake(tile_t *tile);

/** @brief  Software reset. Must call init() again after. */
void tile_disp_rgbw_reset(tile_t *tile);

#endif /* INC_TILE_DISP_RGBW_H_ */
