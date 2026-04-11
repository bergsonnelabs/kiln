/**
 * @file   tile_drive_dc_h.c
 * @brief  H-bridge DC motor driver implementation (DRV8214).
 */

#include "tile_drive_dc_h.h"
#include <stddef.h>

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

static const uint8_t id_table[] = {
    0x34,  /* instance 0 — A1=Hi-Z, A0=Hi-Z (default) */
    0x30,  /* instance 1 — A1=0,    A0=0    */
    0x31,  /* instance 2 — A1=0,    A0=Hi-Z */
    0x32,  /* instance 3 — A1=0,    A0=1    */
    0x33,  /* instance 4 — A1=Hi-Z, A0=0    */
    0x35,  /* instance 5 — A1=Hi-Z, A0=1    */
    0x36,  /* instance 6 — A1=1,    A0=0    */
    0x37,  /* instance 7 — A1=1,    A0=Hi-Z */
    0x38,  /* instance 8 — A1=1,    A0=1    */
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

/* CONFIG4 base: STALL_REP=1, CBC_REP=1, PMODE=1 (PWM), I2C_BC=1 */
#define CONFIG4_BASE  0x3C

/* CS_GAIN_SEL → maximum current in mA */
static const uint16_t cs_max_ma[] = {
    4000,  /* 000b */
    2000,  /* 001b */
    1000,  /* 010b */
    500,   /* 011b */
    250,   /* 100b */
    125,   /* 101b */
    250,   /* 110b (same as 100b, bit 1 don't care) */
    125,   /* 111b (same as 101b, bit 1 don't care) */
};

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

uint8_t tile_drive_dc_h_find(tiles_pal_t* hal, uint8_t instance)
{
    uint8_t id = resolve_id(instance);
    if (id == 0x00) return 0;
    return (hal->i2c_is_ready(hal->handle, id) == 0) ? 1 : 0;
}

void tile_drive_dc_h_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                          const drive_dc_h_cfg_t *cfg)
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

    /* Parse config, apply defaults */
    uint8_t  mode            = DRIVE_DC_H_MODE_VOLTAGE;
    uint8_t  vm_gain         = 1;    /* 0-3.92V range */
    uint8_t  cs_gain         = 0;    /* 4A max */
    uint8_t  target          = 0xFF; /* full scale */
    uint16_t motor_mohm      = 0;
    uint8_t  ripples_per_rev = 12;
    uint16_t kv_uv_per_rpm   = 0;

    if (cfg != NULL) {
        mode    = cfg->mode;
        vm_gain = cfg->vm_gain ? 1 : 0;
        cs_gain = cfg->cs_gain;
        if (cs_gain > 5) cs_gain = 5;
        target  = cfg->target;
        motor_mohm      = cfg->motor_mohm;
        ripples_per_rev = cfg->ripples_per_rev;
        if (ripples_per_rev == 0) ripples_per_rev = 12;
        kv_uv_per_rpm   = cfg->kv_uv_per_rpm;
    }

    /* ---- CONFIG4: I2C bridge control, PWM mode ----
     * [7:6] RC_REP     = 00  (no ripple count on nFAULT)
     * [5]   STALL_REP  = 1   (report stall on nFAULT)
     * [4]   CBC_REP    = 1   (report current regulation on nFAULT)
     * [3]   PMODE      = 1   (PWM mode — supports coast state)
     * [2]   I2C_BC     = 1   (I2C controls bridge)
     * [1]   I2C_EN_IN1 = 0   (start in coast)
     * [0]   I2C_PH_IN2 = 0                                        */
    drv_write(tile, DRV8214_REG_CONFIG4, CONFIG4_BASE);

    /* ---- CONFIG0: enable output stage, faults, voltage range ----
     * [7]   EN_OUT       = 1   (enable output FETs)
     * [6]   EN_OVP       = 1   (overvoltage protection on)
     * [5]   EN_STALL     = 1   (stall detection on)
     * [4]   VSNS_SEL     = 0   (analog output filter)
     * [3]   VM_GAIN_SEL  = cfg (voltage range selection)
     * [2]   CLR_CNT      = 0
     * [1]   CLR_FLT      = 1   (clear any power-on faults)
     * [0]   DUTY_CTRL    = 0   (internal duty control)             */
    drv_write(tile, DRV8214_REG_CONFIG0,
              0xE2 | (vm_gain << 3));

    /* ---- CONFIG3: stall/current regulation settings ----
     * [7:6] IMODE     = 01  (current reg during tINRUSH only)
     * [5]   SMODE     = 1   (stall = indication only, outputs stay on)
     * [4]   INT_VREF  = 1   (use internal 500 mV stall reference)
     * [3]   TBLANK    = 0   (1.8 us blanking time)
     * [2]   TDEG      = 0   (2 us deglitch time)
     * [1]   OCP_MODE  = 1   (auto-retry on overcurrent)
     * [0]   TSD_MODE  = 1   (auto-retry on thermal shutdown)        */
    drv_write(tile, DRV8214_REG_CONFIG3, 0x73);

    /* ---- REG_CTRL0: regulation mode, soft-start, PWM freq ----
     * [7:6] RSVD      = 00
     * [5]   EN_SS     = 1   (soft-start/stop enabled)
     * [4:3] REG_CTRL  = mode-dependent
     * [2]   PWM_FREQ  = 1   (25 kHz)
     * [1:0] W_SCALE   = 11  (128)                                  */
    uint8_t reg_ctrl_bits;
    if (mode == DRIVE_DC_H_MODE_SPEED) {
        reg_ctrl_bits = 0x02;  /* 10b = speed regulation */
    } else {
        reg_ctrl_bits = 0x03;  /* 11b = voltage regulation (also for RIPPLE_COUNT) */
    }
    drv_write(tile, DRV8214_REG_CTRL0,
              0x20 | (reg_ctrl_bits << 3) | 0x07);

    /* ---- REG_CTRL1: target voltage or speed ---- */
    drv_write(tile, DRV8214_REG_CTRL1, target);

    /* ---- CONFIG1/CONFIG2: inrush time (TINRUSH) ----
     * Set to ~100 ms. Each LSB = 102.4 us.
     * 100 ms / 102.4 us ≈ 977 = 0x03D1                             */
    drv_write(tile, DRV8214_REG_CONFIG1, 0xD1);  /* TINRUSH[7:0]  */
    drv_write(tile, DRV8214_REG_CONFIG2, 0x03);  /* TINRUSH[15:8] */

    /* ---- RC_CTRL0: current gain, ripple counting ----
     * [7]   EN_RC        = mode-dependent (enable for speed reg)
     * [6]   DIS_EC       = 0   (error correction enabled)
     * [5]   RC_HIZ       = 0   (bridge stays on at threshold)
     * [4:3] FLT_GAIN_SEL = 01  (gain = 4)
     * [2:0] CS_GAIN_SEL  = cfg                                     */
    uint8_t en_rc = (mode == DRIVE_DC_H_MODE_SPEED ||
                     mode == DRIVE_DC_H_MODE_RIPPLE_COUNT) ? 0x80 : 0x00;
    drv_write(tile, DRV8214_REG_RC_CTRL0,
              en_rc | 0x08 | (cs_gain & 0x07));

    /* ---- REG_CTRL2: output filter ----
     * [7:6] OUT_FLT  = 11  (1000 Hz cutoff — 20x below 25 kHz PWM)
     * [5:0] EXT_DUTY = 0   (not used in I2C bridge mode)           */
    drv_write(tile, DRV8214_REG_CTRL2, 0xC0);

    /* ---- Ripple counting tuning (motor-specific parameters) ----
     * Computes INV_R and KMC register values from motor parameters.
     * Skipped if motor_mohm == 0.                                   */
    if (motor_mohm > 0) {

        /* INV_R = INV_R_SCALE / R_motor (ohms)
         *       = INV_R_SCALE * 1000 / motor_mohm
         * Try scales from largest to smallest for best precision. */
        static const uint32_t inv_r_scales[]    = { 8192, 1024, 64, 2 };
        static const uint8_t  inv_r_scale_bits[] = { 3, 2, 1, 0 };
        uint8_t inv_r = 0;
        uint8_t inv_r_sb = 0;

        for (uint8_t i = 0; i < 4; i++) {
            uint32_t v = inv_r_scales[i] * 1000 / motor_mohm;
            if (v >= 1 && v <= 255) {
                inv_r = (uint8_t)v;
                inv_r_sb = inv_r_scale_bits[i];
                break;
            }
        }

        /* KMC = (Kv / N_R) * KMC_SCALE
         * Kv in V/(rad/s) = kv_uv_per_rpm / 104720 (approx)
         * Pre-computed multipliers: KMC_SCALE * 60 / (2*pi*1e6)
         *   scale 3 (196608): ×1878/1000
         *   scale 2 (98304):  ×939/1000
         *   scale 1 (12288):  ×117/1000
         *   scale 0 (6144):   ×59/1000                              */
        uint8_t kmc = 0;
        uint8_t kmc_sb = 0;

        if (kv_uv_per_rpm > 0) {
            static const uint16_t kmc_mults[]      = { 1878, 939, 117, 59 };
            static const uint8_t  kmc_scale_bits[] = { 3, 2, 1, 0 };

            for (uint8_t i = 0; i < 4; i++) {
                uint32_t num = (uint32_t)kv_uv_per_rpm * kmc_mults[i];
                uint32_t den = (uint32_t)1000 * ripples_per_rev;
                uint32_t v   = (num + den / 2) / den;  /* round */
                if (v >= 1 && v <= 255) {
                    kmc = (uint8_t)v;
                    kmc_sb = kmc_scale_bits[i];
                    break;
                }
            }
        }

        /* RC_CTRL2: scaling factors
         * [7:6] INV_R_SCALE
         * [5:4] KMC_SCALE
         * [3:2] RC_THR_SCALE = 11 (×64, default)
         * [1:0] RC_THR[9:8]  = 11 (default)                        */
        drv_write(tile, DRV8214_REG_RC_CTRL2,
                  (inv_r_sb << 6) | (kmc_sb << 4) | 0x0F);

        /* RC_CTRL3: INV_R */
        if (inv_r > 0) {
            drv_write(tile, DRV8214_REG_RC_CTRL3, inv_r);
        }

        /* RC_CTRL4: KMC */
        if (kmc > 0) {
            drv_write(tile, DRV8214_REG_RC_CTRL4, kmc);
        }
    }

    tile->state = TILE_STATE_READY;
}

/* ---- Motor control ---- */

void tile_drive_dc_h_forward(tile_t* tile)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "forward: not ready");
        return;
    }
    /* PWM mode: IN1=1, IN2=0 → Forward (OUT1=H, OUT2=L) */
    drv_write(tile, DRV8214_REG_CONFIG4, CONFIG4_BASE | 0x02);
}

void tile_drive_dc_h_reverse(tile_t* tile)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "reverse: not ready");
        return;
    }
    /* PWM mode: IN1=0, IN2=1 → Reverse (OUT1=L, OUT2=H) */
    drv_write(tile, DRV8214_REG_CONFIG4, CONFIG4_BASE | 0x01);
}

void tile_drive_dc_h_brake(tile_t* tile)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "brake: not ready");
        return;
    }
    /* PWM mode: IN1=1, IN2=1 → Brake (low-side slow decay) */
    drv_write(tile, DRV8214_REG_CONFIG4, CONFIG4_BASE | 0x03);
}

void tile_drive_dc_h_coast(tile_t* tile)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "coast: not ready");
        return;
    }
    /* PWM mode: IN1=0, IN2=0 → Coast (Hi-Z) */
    drv_write(tile, DRV8214_REG_CONFIG4, CONFIG4_BASE);
}

/* ---- Regulation ---- */

void tile_drive_dc_h_set_target(tile_t* tile, uint8_t value)
{
    if (tile->state != TILE_STATE_READY) {
        TILE_ON_ERROR(tile, "set_target: not ready");
        return;
    }
    drv_write(tile, DRV8214_REG_CTRL1, value);
}

/* ---- Monitoring ---- */

uint8_t tile_drive_dc_h_get_fault(tile_t* tile)
{
    return drv_read(tile, DRV8214_REG_FAULT);
}

void tile_drive_dc_h_clear_fault(tile_t* tile)
{
    uint8_t cfg0 = drv_read(tile, DRV8214_REG_CONFIG0);
    drv_write(tile, DRV8214_REG_CONFIG0, cfg0 | 0x02);  /* CLR_FLT */
}

uint8_t tile_drive_dc_h_is_stalled(tile_t* tile)
{
    return (drv_read(tile, DRV8214_REG_FAULT) & DRV8214_FAULT_STALL) ? 1 : 0;
}

uint16_t tile_drive_dc_h_get_voltage_mv(tile_t* tile)
{
    uint8_t raw = drv_read(tile, DRV8214_REG_REG_STATUS1);
    if (raw == 0) return 0;

    /* VMTR scales with VM_GAIN_SEL (CONFIG0 bit 3):
     *   0 → 0-15.7 V range (0xFF = 15700 mV)
     *   1 → 0-3.92 V range (0xFF = 3920 mV)  */
    uint8_t cfg0 = drv_read(tile, DRV8214_REG_CONFIG0);
    uint16_t fs = (cfg0 & 0x08) ? 3920 : 15700;

    return (uint16_t)((uint32_t)raw * fs / 255);
}

uint16_t tile_drive_dc_h_get_current_ma(tile_t* tile)
{
    uint8_t imtr = drv_read(tile, DRV8214_REG_REG_STATUS2);
    if (imtr == 0) return 0;

    /* Read CS_GAIN_SEL to determine current range */
    uint8_t rc_ctrl0 = drv_read(tile, DRV8214_REG_RC_CTRL0);
    uint8_t cs_sel = rc_ctrl0 & 0x07;

    /* IMTR: 0x00 = 0 A, 0xC0 = max current for gain setting
     * mA = imtr * max_mA / 192 */
    uint16_t max_ma = cs_max_ma[cs_sel];
    return (uint16_t)((uint32_t)imtr * max_ma / 192);
}

uint8_t tile_drive_dc_h_get_speed(tile_t* tile)
{
    return drv_read(tile, DRV8214_REG_RC_STATUS1);
}

uint16_t tile_drive_dc_h_get_ripple_count(tile_t* tile)
{
    uint8_t lo = drv_read(tile, DRV8214_REG_RC_STATUS2);
    uint8_t hi = drv_read(tile, DRV8214_REG_RC_STATUS3);
    return ((uint16_t)hi << 8) | lo;
}

void tile_drive_dc_h_clear_ripple_count(tile_t* tile)
{
    uint8_t cfg0 = drv_read(tile, DRV8214_REG_CONFIG0);
    drv_write(tile, DRV8214_REG_CONFIG0, cfg0 | 0x04);  /* CLR_CNT */
}

/* ---- Power management ---- */

void tile_drive_dc_h_sleep(tile_t* tile)
{
    if (tile->state != TILE_STATE_READY) return;

    uint8_t cfg0 = drv_read(tile, DRV8214_REG_CONFIG0);
    drv_write(tile, DRV8214_REG_CONFIG0, cfg0 & ~0x80);  /* EN_OUT = 0 */
    tile->state = TILE_STATE_SLEEPING;
}

void tile_drive_dc_h_wake(tile_t* tile)
{
    if (tile->state != TILE_STATE_SLEEPING) return;

    uint8_t cfg0 = drv_read(tile, DRV8214_REG_CONFIG0);
    drv_write(tile, DRV8214_REG_CONFIG0, cfg0 | 0x80);   /* EN_OUT = 1 */
    tile->hal->delay_ms(1);  /* tWAKE < 410 us */
    tile->state = TILE_STATE_READY;
}
