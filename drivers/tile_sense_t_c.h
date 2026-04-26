/**
 * @file   tile_sense_t_c.h
 * @brief  Capacitive touch/proximity driver for the Sense.T.C tile (IQS323).
 * @version 1.0.0
 *
 * Azoteq IQS323 ProxFusion controller with self-capacitive touch sensing.
 * The tile's entire top surface is a touch electrode (CH1/CRx1), and a
 * second channel (CH0/CRx0) is available via pad 8 for external sensors.
 *
 * Two communication modes:
 *   - **Polled mode** (default): call process() from your main loop.
 *     No RDY pin wiring required.
 *   - **RDY mode**: provide a rdy_pin in config. EXTI falling-edge ISR
 *     sets a flag; process() only does I2C when the flag is set.
 *     Lower latency, less wasted I2C traffic.
 *
 * Event callback: register an on_event callback to be notified of
 * touch, proximity, slider, and gesture changes without polling
 * individual status functions.
 *
 * Simple polling example (Cores SDK):
 * @code
 *   tile_t touch;
 *   tile_sense_t_c_init(core_tiles_pal(&core_i2c3), 0, &touch, NULL);
 *   while (1) {
 *       tile_sense_t_c_process(&touch);
 *       if (tile_sense_t_c_is_touched(&touch, SENSE_T_C_CH_SURFACE))
 *           led_on();
 *       else
 *           led_off();
 *       core_delay_ms(30);
 *   }
 * @endcode
 *
 * Callback example (polled):
 * @code
 *   void on_touch(tile_t *t, uint16_t status, void *ctx) {
 *       if (status & SENSE_T_C_SURFACE_TOUCH)
 *           led_green();
 *       else
 *           led_off();
 *   }
 *
 *   sense_t_c_cfg_t cfg = { .on_event = on_touch };
 *   tile_sense_t_c_init(core_tiles_pal(&core_i2c3), 0, &touch, &cfg);
 *   while (1) {
 *       tile_sense_t_c_process(&touch);
 *       core_delay_ms(30);
 *   }
 * @endcode
 *
 * Callback example (RDY interrupt):
 * @code
 *   sense_t_c_cfg_t cfg = {
 *       .rdy_pin = 3,
 *       .on_event = on_touch,
 *   };
 *   tile_sense_t_c_init(core_tiles_pal(&core_i2c3), 0, &touch, &cfg);
 *   while (1) {
 *       tile_sense_t_c_process(&touch);
 *       // ... other work — process() returns fast if no RDY event ...
 *   }
 * @endcode
 *
 * Slider note: slider functionality requires multiple channels enabled
 * (e.g. two Sense.T.C tiles chained, or pad 8 connected to an external
 * electrode). A single tile surface alone cannot produce a slider output.
 *
 * Datasheet: Azoteq IQS323, Rev v1.11, August 2025
 *
 * @tessera tile label=Sense.T.C icon=◕
 */

#ifndef INC_TILE_SENSE_T_C_H_
#define INC_TILE_SENSE_T_C_H_

#include "tiles.h"
#include <stdint.h>

/* ================================================================
 * Driver version
 * ================================================================ */

#define TILE_SENSE_T_C_VERSION_MAJOR  1
#define TILE_SENSE_T_C_VERSION_MINOR  0
#define TILE_SENSE_T_C_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);

/* ================================================================
 * Instance mapping
 * ================================================================ */

/**
 * | Instance | Address | Order code  |
 * |----------|---------|-------------|
 * | 0        | 0x44    | IQS323-001  |
 * | 1        | 0x58    | IQS323-002  |
 */
#define IQS323_I2C_ADDR_001   0x44
#define IQS323_I2C_ADDR_002   0x58

/* ================================================================
 * Channel identifiers
 * ================================================================ */

#define SENSE_T_C_CH0           0   /**< CRx0/CTx0 -- pad 8 (external) */
#define SENSE_T_C_CH1           1   /**< CRx1/CTx1 -- tile top surface  */
#define SENSE_T_C_CH2           2   /**< CRx2/CTx2/Bias                 */
#define SENSE_T_C_CH_SURFACE    SENSE_T_C_CH1
#define SENSE_T_C_CH_EXTERNAL   SENSE_T_C_CH0
#define SENSE_T_C_NUM_CHANNELS  3

/** Status bit masks for the tile surface (CH1) — use with status from callback. */
#define SENSE_T_C_SURFACE_TOUCH  IQS323_STATUS_CH_TOUCH(SENSE_T_C_CH_SURFACE)
#define SENSE_T_C_SURFACE_PROX   IQS323_STATUS_CH_PROX(SENSE_T_C_CH_SURFACE)

/** Status bit masks for the external pad (CH0) — pad 8. */
#define SENSE_T_C_EXTERNAL_TOUCH IQS323_STATUS_CH_TOUCH(SENSE_T_C_CH_EXTERNAL)
#define SENSE_T_C_EXTERNAL_PROX  IQS323_STATUS_CH_PROX(SENSE_T_C_CH_EXTERNAL)

/* ================================================================
 * Register map -- 8-bit addresses, 16-bit LE data
 * ================================================================ */

/* Version / ID */
#define IQS323_REG_PRODUCT_NUM       0x00
#define IQS323_REG_MAJOR_VER        0x01
#define IQS323_REG_MINOR_VER        0x02
#define IQS323_REG_HARDWARE_ID      0xE1

/* System (read-only) */
#define IQS323_REG_SYSTEM_STATUS    0x10
#define IQS323_REG_GESTURE_STATUS   0x11
#define IQS323_REG_SLIDER_POSITION  0x12

/* Per-channel data (read-only) */
#define IQS323_REG_CH0_COUNTS       0x13
#define IQS323_REG_CH0_LTA          0x14
#define IQS323_REG_CH1_COUNTS       0x15
#define IQS323_REG_CH1_LTA          0x16
#define IQS323_REG_CH2_COUNTS       0x17
#define IQS323_REG_CH2_LTA          0x18

/* Sensor setup per channel (CH0=0x3x, CH1=0x4x, CH2=0x5x) */
#define IQS323_REG_SENSOR_SETUP(ch) (0x30 + (ch) * 0x10)
#define IQS323_REG_CONV_FREQ(ch)    (0x31 + (ch) * 0x10)
#define IQS323_REG_PROX_CTRL(ch)    (0x32 + (ch) * 0x10)
#define IQS323_REG_PROX_INPUT(ch)   (0x33 + (ch) * 0x10)
#define IQS323_REG_PATTERN_DEF(ch)  (0x34 + (ch) * 0x10)
#define IQS323_REG_PATTERN_SEL(ch)  (0x35 + (ch) * 0x10)
#define IQS323_REG_ATI_SETUP(ch)    (0x36 + (ch) * 0x10)
#define IQS323_REG_ATI_BASE(ch)     (0x37 + (ch) * 0x10)

/* Channel UI setup (CH0=0x6x, CH1=0x7x, CH2=0x8x) */
#define IQS323_REG_CH_SETUP(ch)          (0x60 + (ch) * 0x10)
#define IQS323_REG_PROX_SETTINGS(ch)     (0x61 + (ch) * 0x10)
#define IQS323_REG_TOUCH_SETTINGS(ch)    (0x62 + (ch) * 0x10)
#define IQS323_REG_FOLLOWER_WEIGHT(ch)   (0x63 + (ch) * 0x10)

/* Slider config */
#define IQS323_REG_SLIDER_SETUP     0x90
#define IQS323_REG_SLIDER_CALIB     0x91
#define IQS323_REG_ENABLE_MASK      0x94
#define IQS323_REG_DELTA_LINK(ch)   (0x96 + (ch))

/* Gesture config */
#define IQS323_REG_GESTURE_ENABLE   0xA0

/* Filter betas */
#define IQS323_REG_COUNTS_FILTER    0xB0
#define IQS323_REG_LTA_FILTER       0xB1

/* System control */
#define IQS323_REG_SYSTEM_CONTROL   0xC0
#define IQS323_REG_NP_REPORT_RATE   0xC1
#define IQS323_REG_LP_REPORT_RATE   0xC2
#define IQS323_REG_ULP_REPORT_RATE  0xC3
#define IQS323_REG_HALT_REPORT_RATE 0xC4
#define IQS323_REG_POWER_TIMEOUT    0xC5

/* General */
#define IQS323_REG_I2C_TIMEOUT      0xD1
#define IQS323_REG_EVENT_TIMEOUTS   0xD2
#define IQS323_REG_EVENTS_ENABLE    0xD3
#define IQS323_REG_I2C_SETTINGS     0xE0

/* ================================================================
 * System Status (0x10) bit masks
 * ================================================================ */

#define IQS323_STATUS_PROX_EVENT    (1U << 0)
#define IQS323_STATUS_TOUCH_EVENT   (1U << 1)
#define IQS323_STATUS_SLIDER_EVENT  (1U << 2)
#define IQS323_STATUS_POWER_EVENT   (1U << 3)
#define IQS323_STATUS_ATI_EVENT     (1U << 4)
#define IQS323_STATUS_ATI_ACTIVE    (1U << 5)
#define IQS323_STATUS_ATI_ERROR     (1U << 6)
#define IQS323_STATUS_RESET_EVENT   (1U << 7)

#define IQS323_STATUS_CH0_PROX      (1U << 8)
#define IQS323_STATUS_CH0_TOUCH     (1U << 9)
#define IQS323_STATUS_CH1_PROX      (1U << 10)
#define IQS323_STATUS_CH1_TOUCH     (1U << 11)
#define IQS323_STATUS_CH2_PROX      (1U << 12)
#define IQS323_STATUS_CH2_TOUCH     (1U << 13)
#define IQS323_STATUS_POWER_MASK    (3U << 14)
#define IQS323_STATUS_POWER_SHIFT   14

/** Per-channel touch/prox masks indexed by channel number. */
#define IQS323_STATUS_CH_PROX(ch)   (1U << (8 + (ch) * 2))
#define IQS323_STATUS_CH_TOUCH(ch)  (1U << (9 + (ch) * 2))

/* Invalid communication response (read outside window) */
#define IQS323_INVALID_RESPONSE     0xEEEE

/* ================================================================
 * Gesture Status (0x11) bit masks
 * ================================================================ */

#define IQS323_GESTURE_TAP          (1U << 0)
#define IQS323_GESTURE_SWIPE_POS    (1U << 1)
#define IQS323_GESTURE_SWIPE_NEG    (1U << 2)
#define IQS323_GESTURE_FLICK_POS    (1U << 3)
#define IQS323_GESTURE_FLICK_NEG    (1U << 4)
#define IQS323_GESTURE_HOLD         (1U << 5)
#define IQS323_GESTURE_EVENT        (1U << 6)

/* ================================================================
 * System Control (0xC0) bits
 * ================================================================ */

#define IQS323_CTRL_ACK_RESET       (1U << 0)
#define IQS323_CTRL_SOFT_RESET      (1U << 1)
#define IQS323_CTRL_RE_ATI          (1U << 2)
#define IQS323_CTRL_RESEED          (1U << 3)
#define IQS323_CTRL_POWER_SHIFT     4
#define IQS323_CTRL_POWER_MASK      (7U << 4)
#define IQS323_CTRL_INTERFACE_SEL   (1U << 7)

/* ================================================================
 * Enums
 * ================================================================ */

typedef enum {
    SENSE_T_C_POWER_NORMAL       = 0,  /**< Normal power mode */
    SENSE_T_C_POWER_LOW          = 1,  /**< Low power mode */
    SENSE_T_C_POWER_ULTRA_LOW    = 2,  /**< Ultra-low power mode */
    SENSE_T_C_POWER_HALT         = 3,  /**< Halt (lowest power) */
    SENSE_T_C_POWER_AUTO         = 4,  /**< Auto power mode switching */
    SENSE_T_C_POWER_AUTO_NO_ULP  = 5,  /**< Auto mode, skip ultra-low power */
} sense_t_c_power_mode_t;

/* ================================================================
 * Event callback
 * ================================================================ */

/**
 * Event callback fired by tile_sense_t_c_process() when the device
 * reports new touch, proximity, or gesture events.
 *
 * @param tile    The tile instance
 * @param status  System Status register value (use SENSE_T_C_SURFACE_TOUCH etc.)
 * @param ctx     User context (from config or on_event registration)
 *
 * Always runs in main-loop context (never ISR). I2C is safe to call.
 */
typedef void (*sense_t_c_event_cb_t)(tile_t *tile, uint16_t status, void *ctx);

/* ================================================================
 * Configuration
 * ================================================================ */

/**
 * Optional init config. Pass NULL for defaults (polled mode, no callback).
 */
typedef struct {
    /* RDY pin — set to enable interrupt-driven mode.
     * The RDY pin (pad 3) has a 4.7k pull-up and 100nF pull-down on the tile.
     * Uses hal->gpio_irq_enable() to register the falling-edge ISR.
     * Set to 0 for polled mode (no RDY pin needed). */
    uint8_t rdy_pin;              /**< Platform pin for RDY. 0 = polled mode. */

    /* Event callback — fired by process() on touch/prox/gesture changes */
    sense_t_c_event_cb_t on_event;   /**< Callback. NULL = no callback. */
    void *event_ctx;              /**< User context passed to callback. */

    uint8_t channels;             /**< Channel enable bitmask (bit 0=CH0, 1=CH1, 2=CH2). 0 = factory default (all enabled). */

    /* Thresholds (0 = use factory defaults) */
    uint8_t prox_threshold;       /**< Proximity threshold (factory default ~4). */
    uint8_t touch_threshold;      /**< Touch threshold (factory default ~8). */
} sense_t_c_cfg_t;

/* Hardware ID values */
#define IQS323_HW_ID_3DD   0xF003
#define IQS323_HW_ID_3ED   0xF004

/* ================================================================
 * Public API
 * ================================================================ */

/**
 * @brief  Probe the bus for an IQS323 touch controller.
 * @param  hal       Tiles HAL handle (I2C bus)
 * @param  instance  Device instance (0 or 1, selects I2C address)
 * @return 1 if found, 0 if not
 */
uint8_t tile_sense_t_c_find(tiles_pal_t *hal, uint8_t instance);

/**
 * @brief  Initialize the touch controller.
 *
 * Soft resets, configures CH1 (surface) for self-capacitive touch,
 * sets proximity/touch thresholds, runs ATI.
 *
 * @param  hal       Tiles HAL handle (I2C bus)
 * @param  instance  Device instance (0 or 1, selects I2C address)
 * @param  tile      Tile handle to initialize
 * @param  cfg       Optional config (thresholds, RDY pin, callback). NULL for defaults.
 */
void tile_sense_t_c_init(tiles_pal_t *hal, uint8_t instance,
                         tile_t *tile, const sense_t_c_cfg_t *cfg);

/* ---- Event processing ---- */

/**
 * @brief  Read pending touch/proximity events and update internal state.
 * @tessera expose category=tile name=process
 *
 * Call from your main loop.
 * In polled mode: reads status register, fires callback if events present.
 * In RDY mode: returns immediately if no RDY interrupt; reads + fires
 * callback only when the device has new data.
 *
 * @param  tile  Tile handle
 */
void tile_sense_t_c_process(tile_t *tile);

/**
 * @brief  Register or change the event callback for touch/proximity events.
 * @param  tile  Tile handle
 * @param  cb    Callback function (NULL to disable)
 * @param  ctx   User context passed to callback
 */
void tile_sense_t_c_on_event(tile_t *tile, sense_t_c_event_cb_t cb, void *ctx);

/* ---- Lifecycle ---- */

/**
 * @brief  Enter low-power sleep mode.
 * @tessera expose category=tile name=sleep
 */
void tile_sense_t_c_sleep(tile_t *tile);

/**
 * @brief  Wake from sleep mode.
 * @tessera expose category=tile name=wake
 */
void tile_sense_t_c_wake(tile_t *tile);

void tile_sense_t_c_reset(tile_t *tile);

/* ---- Status (from last process() call) ---- */

/**
 * @brief  Read the cached System Status word from the last process() call.
 * @tessera expose category=tile name=get_status returns=int
 */
uint16_t tile_sense_t_c_get_status(tile_t *tile);

/**
 * @brief  Read the cached Gesture Status word from the last process() call.
 * @tessera expose category=tile name=get_gestures returns=int
 */
uint16_t tile_sense_t_c_get_gestures(tile_t *tile);

/**
 * @brief  Check if the given channel is currently touched.
 * @tessera expose category=tile name=is_touched returns=bool
 */
uint8_t  tile_sense_t_c_is_touched(tile_t *tile, uint8_t channel);

/**
 * @brief  Check if the given channel currently reports proximity.
 * @tessera expose category=tile name=is_prox returns=bool
 */
uint8_t  tile_sense_t_c_is_prox(tile_t *tile, uint8_t channel);

/* ---- Data ---- */

/**
 * @brief  Read the raw counts value for a channel.
 * @tessera expose category=tile name=get_counts returns=int
 */
uint16_t tile_sense_t_c_get_counts(tile_t *tile, uint8_t channel);

/**
 * @brief  Read the long-term average (LTA) for a channel.
 * @tessera expose category=tile name=get_lta returns=int
 */
uint16_t tile_sense_t_c_get_lta(tile_t *tile, uint8_t channel);

/**
 * @brief  Read the delta (counts - LTA) for a channel.
 * @tessera expose category=tile name=get_delta returns=int
 */
int16_t  tile_sense_t_c_get_delta(tile_t *tile, uint8_t channel);

/**
 * @brief  Read the slider position (if a slider is configured).
 * @tessera expose category=tile name=get_slider returns=int
 */
uint16_t tile_sense_t_c_get_slider(tile_t *tile);

/* ---- Configuration ---- */

/**
 * @brief  Set proximity and touch thresholds for a channel.
 * @tessera expose category=tile name=set_thresholds
 */
void tile_sense_t_c_set_thresholds(tile_t *tile, uint8_t channel,
                                   uint8_t prox_thresh, uint8_t touch_thresh);

/**
 * @brief  Set the device power mode.
 * @tessera expose category=tile name=set_power_mode
 */
void tile_sense_t_c_set_power_mode(tile_t *tile, sense_t_c_power_mode_t mode);
void tile_sense_t_c_enable_events(tile_t *tile, uint16_t mask);
void tile_sense_t_c_ati(tile_t *tile);

/* ---- Low-level ---- */

uint16_t tile_sense_t_c_read_reg(tile_t *tile, uint8_t reg);
void tile_sense_t_c_write_reg(tile_t *tile, uint8_t reg, uint16_t value);

#endif /* INC_TILE_SENSE_T_C_H_ */
