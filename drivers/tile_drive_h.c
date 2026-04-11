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

/**
 * @brief  Poll the GO bit until it self-clears or timeout.
 * @return 1 if GO cleared (process complete), 0 if timeout.
 */
static uint8_t drv_wait_go(tile_t* tile, uint16_t timeout_ms)
{
    while (timeout_ms > 0) {
        if ((drv_read(tile, DRV2605L_REG_GO) & 0x01) == 0) return 1;
        tile->hal->delay_ms(10);
        if (timeout_ms <= 10) break;
        timeout_ms -= 10;
    }
    return 0;
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

void tile_drive_h_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                       const drive_h_cfg_t *cfg)
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
    drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_INTERNAL_TRIG);
    hal->delay_ms(400);

    /* Parse config, apply defaults for the Drive.H onboard LRA
     * (0.7 Vrms rated, ~260 Hz resonance, coin-type). */
    uint8_t closed_loop = 0;
    uint8_t library = 6;
    uint8_t rated_v = 0x1A;   /* 0.7 Vrms */
    uint8_t od_clamp = 0x25;
    if (cfg != NULL) {
        if (cfg->library >= 1 && cfg->library <= 6) {
            library = cfg->library;
        }
        closed_loop = cfg->closed_loop ? 1 : 0;
        if (cfg->rated_voltage != 0) rated_v  = cfg->rated_voltage;
        if (cfg->od_clamp != 0)      od_clamp = cfg->od_clamp;
    }

    /* LRA drive levels */
    drv_write(tile, DRV2605L_REG_RATED_VOLTAGE, rated_v);
    drv_write(tile, DRV2605L_REG_OD_CLAMP, od_clamp);

    /* Reset auto-cal results to defaults so diagnostics and calibration
     * start from a known state. The DRV2605L retains registers across
     * MCU resets if it stays powered — stale cal values from a prior
     * session would skew back-EMF thresholds. */
    drv_write(tile, DRV2605L_REG_A_CAL_COMP, 0x0D);
    drv_write(tile, DRV2605L_REG_A_CAL_BEMF, 0x6D);

    /* Configure FEEDBACK_CTRL: ERM_LRA bit [7], brake factor, loop gain, BEMF gain */
    uint8_t fb_ctrl = 0xB6;  /* LRA mode, default brake/loop/BEMF */
    if (library <= 5) {
        fb_ctrl = 0x36;      /* ERM mode (N_ERM_LRA = 0) */
    }
    drv_write(tile, DRV2605L_REG_FEEDBACK_CTRL, fb_ctrl);
    hal->delay_ms(100);

    /* Set DRIVE_TIME for the onboard LRA (~260 Hz).
     * Optimal = 0.5 × LRA period = 1.92 ms → DRIVE_TIME = 14 (0x0E).
     * CONTROL1: preserve STARTUP_BOOST [7], set DRIVE_TIME [4:0]. */
    uint8_t ctrl1 = drv_read(tile, DRV2605L_REG_CONTROL1);
    ctrl1 = (ctrl1 & 0xE0) | 0x0E;
    drv_write(tile, DRV2605L_REG_CONTROL1, ctrl1);

    /* Configure CONTROL3: LRA_OPEN_LOOP [0], ERM_OPEN_LOOP [5] */
    uint8_t ctrl3 = drv_read(tile, DRV2605L_REG_CONTROL3);
    if (library == 6) {
        /* LRA mode */
        if (closed_loop) {
            ctrl3 &= ~0x01;  /* LRA_OPEN_LOOP = 0 (closed loop) */
        } else {
            ctrl3 |= 0x01;   /* LRA_OPEN_LOOP = 1 (open loop) */
        }
    } else {
        /* ERM mode */
        if (closed_loop) {
            ctrl3 &= ~0x20;  /* ERM_OPEN_LOOP = 0 (closed loop) */
        } else {
            ctrl3 |= 0x20;   /* ERM_OPEN_LOOP = 1 (open loop) */
        }
    }
    drv_write(tile, DRV2605L_REG_CONTROL3, ctrl3);

    /* Select waveform library */
    drv_write(tile, DRV2605L_REG_LIBRARY_SEL, library);

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

void tile_drive_h_play_sequence(tile_t* tile, const uint8_t *effects,
                                uint8_t count)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "play_sequence: not ready");
        return;
    }
    if (count == 0) return;
    if (count > DRV2605L_SEQ_MAX) count = DRV2605L_SEQ_MAX;

    /* Load effects into sequencer slots */
    for (uint8_t i = 0; i < count; i++) {
        drv_write(tile, DRV2605L_REG_WAVE_SEQ_0 + i, effects[i]);
    }

    /* Terminate sequence (if fewer than 8 effects) */
    if (count < DRV2605L_SEQ_MAX) {
        drv_write(tile, DRV2605L_REG_WAVE_SEQ_0 + count, 0x00);
    }

    /* Trigger playback */
    drv_write(tile, DRV2605L_REG_GO, 0x01);
}

void tile_drive_h_load_sequence(tile_t* tile, const uint8_t *effects,
                                uint8_t count)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "load_sequence: not ready");
        return;
    }
    if (count == 0) return;
    if (count > DRV2605L_SEQ_MAX) count = DRV2605L_SEQ_MAX;

    for (uint8_t i = 0; i < count; i++) {
        drv_write(tile, DRV2605L_REG_WAVE_SEQ_0 + i, effects[i]);
    }

    if (count < DRV2605L_SEQ_MAX) {
        drv_write(tile, DRV2605L_REG_WAVE_SEQ_0 + count, 0x00);
    }
}

void tile_drive_h_set_trigger(tile_t* tile, uint8_t mode)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "set_trigger: not ready");
        return;
    }

    uint8_t reg_mode;
    switch (mode) {
    case DRIVE_H_TRIG_EDGE:
        reg_mode = DRV2605L_MODE_EXT_EDGE;
        break;
    case DRIVE_H_TRIG_LEVEL:
        reg_mode = DRV2605L_MODE_EXT_LEVEL;
        break;
    default:
        reg_mode = DRV2605L_MODE_INTERNAL_TRIG;
        break;
    }
    drv_write(tile, DRV2605L_REG_MODE, reg_mode);
}

uint8_t tile_drive_h_is_playing(tile_t* tile)
{
    return (drv_read(tile, DRV2605L_REG_GO) & 0x01);
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
    drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_RTP);
    drv_write(tile, DRV2605L_REG_RTP, 0x00);
}

void tile_drive_h_rtp_write(tile_t* tile, uint8_t amplitude)
{
    drv_write(tile, DRV2605L_REG_RTP, amplitude);
}

void tile_drive_h_rtp_stop(tile_t* tile)
{
    drv_write(tile, DRV2605L_REG_RTP, 0x00);
    drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_INTERNAL_TRIG);
}

uint8_t tile_drive_h_get_status(tile_t* tile)
{
    return drv_read(tile, DRV2605L_REG_STATUS);
}

uint8_t tile_drive_h_diagnose(tile_t* tile)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "diagnose: not ready");
        return 0;
    }

    /* Diagnostics uses back-EMF sensing (closed-loop path) regardless
     * of normal operating mode. Temporarily ensure closed-loop so the
     * back-EMF thresholds match RATED_VOLTAGE / OD_CLAMP. */
    uint8_t ctrl3_saved = drv_read(tile, DRV2605L_REG_CONTROL3);
    drv_write(tile, DRV2605L_REG_CONTROL3,
              ctrl3_saved & ~0x21);  /* clear LRA_OPEN_LOOP & ERM_OPEN_LOOP */

    /* Reset auto-cal results to defaults so the diagnostic engine
     * evaluates back-EMF from a known baseline. Stale values from
     * a prior calibration can tighten thresholds and cause false
     * failures with small actuators. */
    uint8_t comp_saved = drv_read(tile, DRV2605L_REG_A_CAL_COMP);
    uint8_t bemf_saved = drv_read(tile, DRV2605L_REG_A_CAL_BEMF);
    drv_write(tile, DRV2605L_REG_A_CAL_COMP, 0x0D);
    drv_write(tile, DRV2605L_REG_A_CAL_BEMF, 0x6D);

    /* Enter diagnostics mode */
    drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_DIAGNOSTICS);

    /* Trigger diagnostic routine */
    drv_write(tile, DRV2605L_REG_GO, 0x01);

    /* Wait for completion (up to 2 seconds) */
    if (!drv_wait_go(tile, 2000)) {
        drv_write(tile, DRV2605L_REG_CONTROL3, ctrl3_saved);
        drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_INTERNAL_TRIG);
        return 0;
    }

    /* Read diagnostic result: DIAG_RESULT bit [3] of STATUS.
     * 0 = passed, 1 = failed. */
    uint8_t status = drv_read(tile, DRV2605L_REG_STATUS);
    uint8_t passed = (status & DRV2605L_STATUS_DIAG_RESULT) ? 0 : 1;

    /* Restore cal registers, open/closed-loop setting, and mode */
    drv_write(tile, DRV2605L_REG_A_CAL_COMP, comp_saved);
    drv_write(tile, DRV2605L_REG_A_CAL_BEMF, bemf_saved);
    drv_write(tile, DRV2605L_REG_CONTROL3, ctrl3_saved);
    drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_INTERNAL_TRIG);

    return passed;
}

uint8_t tile_drive_h_calibrate(tile_t* tile)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "calibrate: not ready");
        return 0;
    }

    /* Calibration always uses closed-loop back-EMF feedback.
     * Save and temporarily override open/closed-loop setting. */
    uint8_t ctrl3_saved = drv_read(tile, DRV2605L_REG_CONTROL3);
    drv_write(tile, DRV2605L_REG_CONTROL3,
              ctrl3_saved & ~0x21);  /* clear LRA_OPEN_LOOP & ERM_OPEN_LOOP */

    /* Set TI-recommended calibration parameters.
     * FEEDBACK_CTRL: preserve ERM_LRA bit, set FB_BRAKE_FACTOR=2,
     * LOOP_GAIN=1, BEMF_GAIN=2. */
    uint8_t fb_saved = drv_read(tile, DRV2605L_REG_FEEDBACK_CTRL);
    uint8_t fb_cal = (fb_saved & 0x80) | (2 << 4) | (1 << 2) | 2;
    drv_write(tile, DRV2605L_REG_FEEDBACK_CTRL, fb_cal);

    /* AUTO_CAL_TIME = 3 → 1000-1200 ms for reliable convergence */
    uint8_t ctrl4 = drv_read(tile, DRV2605L_REG_CONTROL4);
    ctrl4 = (ctrl4 & 0x0F) | (3 << 4);
    drv_write(tile, DRV2605L_REG_CONTROL4, ctrl4);

    /* Enter auto-calibration mode */
    drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_CALIBRATION);

    /* Trigger calibration */
    drv_write(tile, DRV2605L_REG_GO, 0x01);

    /* Wait for completion (up to 2 seconds) */
    if (!drv_wait_go(tile, 2000)) {
        drv_write(tile, DRV2605L_REG_FEEDBACK_CTRL, fb_saved);
        drv_write(tile, DRV2605L_REG_CONTROL3, ctrl3_saved);
        drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_INTERNAL_TRIG);
        return 0;
    }

    /* Check DIAG_RESULT: 0 = converged (pass), 1 = failed */
    uint8_t status = drv_read(tile, DRV2605L_REG_STATUS);
    uint8_t passed = (status & DRV2605L_STATUS_DIAG_RESULT) ? 0 : 1;

    /* Restore feedback control and open/closed-loop setting.
     * Note: A_CAL_COMP and A_CAL_BEMF are now populated by the
     * cal engine and persist until reset. */
    drv_write(tile, DRV2605L_REG_FEEDBACK_CTRL, fb_saved);
    drv_write(tile, DRV2605L_REG_CONTROL3, ctrl3_saved);
    drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_INTERNAL_TRIG);

    return passed;
}

uint16_t tile_drive_h_get_vbat_mv(tile_t* tile)
{
    uint8_t raw = drv_read(tile, DRV2605L_REG_VBAT);
    if (raw == 0) return 0;
    /* VBAT (V) = raw * 5.6 / 255
     * In mV:    raw * 5600 / 255
     * Max raw=255 → 255*5600 = 1,428,000 — fits uint32 */
    return (uint16_t)((uint32_t)raw * 5600 / 255);
}

uint16_t tile_drive_h_get_resonance_hz(tile_t* tile)
{
    uint8_t raw = drv_read(tile, DRV2605L_REG_LRA_PERIOD);
    if (raw == 0) return 0;
    /* Period (us) = raw * 98.46
     * Freq (Hz)   = 1e6 / (raw * 98.46)
     *             = 10156 / raw  (integer approximation) */
    return (uint16_t)(10156u / raw);
}

void tile_drive_h_standby(tile_t* tile)
{
    if (tile->state != TILE_STATE_READY) return;
    drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_STANDBY);
    tile->state = TILE_STATE_SLEEPING;
}

void tile_drive_h_wake(tile_t* tile)
{
    if (tile->state != TILE_STATE_SLEEPING) return;
    drv_write(tile, DRV2605L_REG_MODE, DRV2605L_MODE_INTERNAL_TRIG);
    tile->hal->delay_ms(5);
    tile->state = TILE_STATE_READY;
}
