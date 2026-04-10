/**
 * @file   tile_drive_a_2.c
 * @brief  Drive.A.2 (DAC63202W + 2x TPA2028D1) — complete driver implementation.
 *
 * Platform-agnostic. All bus access via tile->hal function pointers.
 * Supports both I2C and SPI for DAC access; amplifier control is
 * I2C-only (TPA2028D1 has no SPI interface).
 *
 * DAC63202W uses 16-bit big-endian registers with 7-bit addresses.
 * TPA2028D1 uses 8-bit registers with 8-bit addresses.
 *
 * Reference: TI SLASF73A (DAC63202W), TI SLOS660C (TPA2028D1)
 */

#include "tile_drive_a_2.h"
#include <stddef.h>

/* ================================================================
 * Instance → I2C address table
 * ================================================================ */

static const uint8_t id_table[] = {
    DAC63202W_I2C_ADDR_DEFAULT,  /* 0: 0x49 (A0 → VDD, default) */
    DAC63202W_I2C_ADDR_GND,      /* 1: 0x48 (A0 → GND) */
    DAC63202W_I2C_ADDR_SDA,      /* 2: 0x4A (A0 → SDA) */
    DAC63202W_I2C_ADDR_SCL,      /* 3: 0x4B (A0 → SCL) */
};

#define NUM_INSTANCES  (sizeof(id_table) / sizeof(id_table[0]))

static uint8_t resolve_id(uint8_t instance)
{
    return (instance < NUM_INSTANCES) ? id_table[instance] : 0;
}

/* ================================================================
 * Per-instance driver state
 * ================================================================ */

typedef struct {
    uint8_t  amp_addr;     /* TPA2028D1 address (0x58), or 0 if unavailable */
    uint8_t  gain[2];      /* Cached VOUT-GAIN-X per DAC channel */
    uint16_t vref_mv;      /* Effective full-scale voltage in mV */
} drive_a_2_state_t;

static drive_a_2_state_t drv_state[NUM_INSTANCES];

static drive_a_2_state_t *state_for(tile_t *tile)
{
    if (tile->hal->buses & TILES_BUS_SPI) {
        uint8_t idx = tile->id < NUM_INSTANCES ? tile->id : 0;
        return &drv_state[idx];
    }
    for (uint8_t i = 0; i < NUM_INSTANCES; i++)
        if (id_table[i] == tile->id) return &drv_state[i];
    return &drv_state[0];
}

/* ================================================================
 * Private helpers
 * ================================================================ */

static void memzero(void *p, uint8_t n)
{
    uint8_t *b = (uint8_t *)p;
    while (n--) *b++ = 0;
}

/* --- DAC register access (bus-aware) --- */

static void dac_write(tile_t *tile, uint8_t reg, uint16_t value)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFF);
    if (tile->hal->buses & TILES_BUS_SPI)
        tile->hal->spi_write(tile->hal->handle, tile->id, reg, buf, 2);
    else
        tile->hal->i2c_write(tile->hal->handle, tile->id, reg, buf, 2);
}

static uint16_t dac_read(tile_t *tile, uint8_t reg)
{
    uint8_t buf[2] = {0, 0};
    if (tile->hal->buses & TILES_BUS_SPI)
        tile->hal->spi_read(tile->hal->handle, tile->id, reg, buf, 2);
    else
        tile->hal->i2c_read(tile->hal->handle, tile->id, reg, buf, 2);
    return ((uint16_t)buf[0] << 8) | buf[1];
}

/* --- Amplifier register access (I2C only) --- */

static void amp_write(tile_t *tile, uint8_t reg, uint8_t value)
{
    drive_a_2_state_t *s = state_for(tile);
    if (!s->amp_addr) return;
    tile->hal->i2c_write(tile->hal->handle, s->amp_addr, reg, &value, 1);
}

static uint8_t amp_read(tile_t *tile, uint8_t reg)
{
    drive_a_2_state_t *s = state_for(tile);
    if (!s->amp_addr) return 0;
    uint8_t val = 0;
    tile->hal->i2c_read(tile->hal->handle, s->amp_addr, reg, &val, 1);
    return val;
}

/* --- Channel → register address helpers --- */

static uint8_t dac_data_reg(uint8_t ch)
{
    return (ch == 0) ? DAC63202W_REG_DAC_0_DATA : DAC63202W_REG_DAC_1_DATA;
}

static uint8_t dac_vout_cmp_reg(uint8_t ch)
{
    return (ch == 0) ? DAC63202W_REG_DAC_0_VOUT_CMP_CFG
                     : DAC63202W_REG_DAC_1_VOUT_CMP_CFG;
}

static uint8_t dac_func_cfg_reg(uint8_t ch)
{
    return (ch == 0) ? DAC63202W_REG_DAC_0_FUNC_CFG
                     : DAC63202W_REG_DAC_1_FUNC_CFG;
}

/* --- Reference voltage resolution --- */

static uint16_t resolve_vref(uint8_t gain_sel)
{
    switch (gain_sel) {
    case DRIVE_A_2_GAIN_1X_EXT:   return 3300;  /* assume 3.3V external */
    case DRIVE_A_2_GAIN_1X_VDD:   return 3300;  /* assume 3.3V VDD */
    case DRIVE_A_2_GAIN_1P5X_INT: return 1815;  /* 1210 * 1.5 */
    case DRIVE_A_2_GAIN_2X_INT:   return 2420;  /* 1210 * 2 */
    case DRIVE_A_2_GAIN_3X_INT:   return 3630;  /* 1210 * 3 */
    case DRIVE_A_2_GAIN_4X_INT:   return 4840;  /* 1210 * 4 */
    default:                      return 3300;
    }
}

/* ================================================================
 * Public API — Lifecycle
 * ================================================================ */

uint8_t tile_drive_a_2_find(tiles_hal_t *hal, uint8_t instance)
{
    if (hal->buses & TILES_BUS_SPI) {
        /* SPI: probe via GENERAL-STATUS read — check DEVICE-ID */
        uint8_t buf[2] = {0, 0};
        hal->spi_read(hal->handle, instance,
                      DAC63202W_REG_GENERAL_STATUS, buf, 2);
        uint16_t status = ((uint16_t)buf[0] << 8) | buf[1];
        return ((status >> 2) & 0x3F) == DAC63202W_DEVICE_ID;
    }

    uint8_t addr = resolve_id(instance);
    if (!addr) return 0;
    return (hal->i2c_is_ready(hal->handle, addr) == 0) ? 1 : 0;
}

void tile_drive_a_2_init(tiles_hal_t *hal, uint8_t instance,
                         tile_t *tile, const drive_a_2_cfg_t *cfg)
{
    memzero(tile, sizeof(tile_t));
    tile->hal = hal;

    if (hal->buses & TILES_BUS_SPI) {
        tile->id = instance;
    } else {
        tile->id = resolve_id(instance);
        if (!tile->id) {
            tile->state = TILE_STATE_ERROR;
            TILE_ON_ERROR(tile, "drive_a_2: invalid instance");
            return;
        }
    }

    /* Probe DAC */
    if (hal->buses & TILES_BUS_SPI) {
        /* SPI: verified by DEVICE-ID read below */
    } else {
        if (hal->i2c_is_ready(hal->handle, tile->id) != 0) {
            tile->state = TILE_STATE_ERROR;
            TILE_ON_ERROR(tile, "drive_a_2: DAC not found on bus");
            return;
        }
    }

    /* Read and verify DEVICE-ID */
    uint16_t status = dac_read(tile, DAC63202W_REG_GENERAL_STATUS);
    uint8_t dev_id = (status >> 2) & 0x3F;
    if (dev_id != DAC63202W_DEVICE_ID) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "drive_a_2: unexpected DAC device ID");
        return;
    }

    /* Initialize per-instance state */
    drive_a_2_state_t *s = state_for(tile);
    memzero(s, sizeof(drive_a_2_state_t));

    /* Apply gain config (default: 1x external VREF — tile has VREF tied to VDD) */
    uint8_t gain_sel = cfg ? cfg->gain : DRIVE_A_2_GAIN_1X_EXT;
    s->gain[0] = gain_sel;
    s->gain[1] = gain_sel;
    s->vref_mv = resolve_vref(gain_sel);

    /* Power up both VOUT channels, power down IOUT.
     * No software reset — preserve NVM defaults for analog config. */
    uint16_t common_cfg = 0x0201;  /* VOUT-PDN-0/1 = 00, IOUT-PDN-0/1 = 1 */
    if (gain_sel >= DRIVE_A_2_GAIN_1P5X_INT)
        common_cfg |= (1 << 12);  /* EN-INT-REF */
    dac_write(tile, DAC63202W_REG_COMMON_CONFIG, common_cfg);

    /* Set both DAC outputs to mid-scale and allow settling.
     * The amp input is referenced to VDD/2, so mid-scale = zero
     * differential input — this prevents amp saturation on wake. */
    dac_write(tile, dac_data_reg(0), 2048 << 4);
    dac_write(tile, dac_data_reg(1), 2048 << 4);
    hal->delay_ms(50);

    /* Set gain on both channels (if non-default) */
    if (gain_sel != DRIVE_A_2_GAIN_1X_EXT) {
        for (uint8_t ch = 0; ch < 2; ch++) {
            uint16_t vout_cfg = dac_read(tile, dac_vout_cmp_reg(ch));
            vout_cfg &= ~(0x07 << 10);
            vout_cfg |= ((uint16_t)gain_sel << 10);
            dac_write(tile, dac_vout_cmp_reg(ch), vout_cfg);
        }
    }

    /* Probe and configure amplifiers (I2C only).
     * The TPA2028D1 AGC can cause saturation if it ramps gain while
     * there is any DC offset at the input.  Sequence:
     *   1. Put amp in software shutdown
     *   2. Configure for fixed gain, AGC compression off
     *   3. Wake amp — DAC is already settled at mid-scale */
    if (!(hal->buses & TILES_BUS_SPI)) {
        if (hal->i2c_is_ready(hal->handle, TPA2028D1_I2C_ADDR) == 0) {
            s->amp_addr = TPA2028D1_I2C_ADDR;

            /* Shutdown amp while configuring */
            amp_write(tile, TPA2028D1_REG_FUNC_CTRL, 0xE2);  /* EN=1, SWS=1, NG_EN=0 */
            hal->delay_ms(5);

            /* Set fixed gain (default 6 dB), AGC compression off */
            int8_t amp_gain = (cfg && cfg->amp_gain_db) ? cfg->amp_gain_db : 6;
            amp_write(tile, TPA2028D1_REG_AGC_GAIN,
                      (uint8_t)(amp_gain & 0x3F));
            amp_write(tile, TPA2028D1_REG_AGC_CTRL2, 0xC0);  /* max 30 dB, comp 1:1 */
            amp_write(tile, TPA2028D1_REG_AGC_CTRL1, 0x80);  /* limiter off */

            /* Wake amp — DAC is settled, no transient */
            amp_write(tile, TPA2028D1_REG_FUNC_CTRL, 0xC2);  /* EN=1, SWS=0, NG_EN=0 */
            hal->delay_ms(10);
        }
    }

    tile->state = TILE_STATE_READY;
}

void tile_drive_a_2_sleep(tile_t *tile)
{
    /* Power down both VOUT channels to Hi-Z */
    uint16_t common_cfg = dac_read(tile, DAC63202W_REG_COMMON_CONFIG);
    common_cfg |= (0x03 << 10);  /* VOUT-PDN-0 = 11 (Hi-Z) */
    common_cfg |= (0x03 << 1);   /* VOUT-PDN-1 = 11 (Hi-Z) */
    dac_write(tile, DAC63202W_REG_COMMON_CONFIG, common_cfg);

    /* Put amps into software shutdown */
    uint8_t reg1 = amp_read(tile, TPA2028D1_REG_FUNC_CTRL);
    reg1 |= (1 << 5);   /* SWS = 1 */
    amp_write(tile, TPA2028D1_REG_FUNC_CTRL, reg1);

    tile->state = TILE_STATE_SLEEPING;
}

void tile_drive_a_2_wake(tile_t *tile)
{
    drive_a_2_state_t *s = state_for(tile);

    /* Power up both VOUT channels, set EN-INT-REF if needed */
    uint16_t common_cfg = 0x0201;
    if (s->gain[0] >= DRIVE_A_2_GAIN_1P5X_INT ||
        s->gain[1] >= DRIVE_A_2_GAIN_1P5X_INT)
        common_cfg |= (1 << 12);
    dac_write(tile, DAC63202W_REG_COMMON_CONFIG, common_cfg);

    /* Settle DAC at mid-scale before waking amp */
    dac_write(tile, dac_data_reg(0), 2048 << 4);
    dac_write(tile, dac_data_reg(1), 2048 << 4);
    tile->hal->delay_ms(50);

    /* Restore gain on both channels */
    for (uint8_t ch = 0; ch < 2; ch++) {
        uint16_t vout_cfg = dac_read(tile, dac_vout_cmp_reg(ch));
        vout_cfg &= ~(0x07 << 10);
        vout_cfg |= ((uint16_t)s->gain[ch] << 10);
        dac_write(tile, dac_vout_cmp_reg(ch), vout_cfg);
    }

    /* Wake amp after DAC is settled */
    amp_write(tile, TPA2028D1_REG_FUNC_CTRL, 0xC2);  /* EN=1, SWS=0, NG_EN=0 */
    tile->hal->delay_ms(10);

    tile->state = TILE_STATE_READY;
}

void tile_drive_a_2_reset(tile_t *tile)
{
    dac_write(tile, DAC63202W_REG_COMMON_TRIGGER, 0x0A00);
    tile->state = TILE_STATE_NONE;
}

/* ================================================================
 * Public API — DAC output
 * ================================================================ */

void tile_drive_a_2_set(tile_t *tile, uint8_t channel, uint16_t value)
{
    if (channel > 1) return;
    if (value > DAC63202W_DAC_MAX) value = DAC63202W_DAC_MAX;
    dac_write(tile, dac_data_reg(channel), (uint16_t)(value << 4));
}

void tile_drive_a_2_set_mv(tile_t *tile, uint8_t channel, uint16_t mv)
{
    if (channel > 1) return;
    drive_a_2_state_t *s = state_for(tile);
    if (s->vref_mv == 0) return;
    uint32_t code = ((uint32_t)mv * 4096) / s->vref_mv;
    if (code > DAC63202W_DAC_MAX) code = DAC63202W_DAC_MAX;
    tile_drive_a_2_set(tile, channel, (uint16_t)code);
}

uint16_t tile_drive_a_2_get(tile_t *tile, uint8_t channel)
{
    if (channel > 1) return 0;
    uint16_t raw = dac_read(tile, dac_data_reg(channel));
    return (raw >> 4) & 0x0FFF;
}

/* ================================================================
 * Public API — DAC configuration
 * ================================================================ */

void tile_drive_a_2_set_gain(tile_t *tile, uint8_t channel,
                             drive_a_2_gain_t gain)
{
    if (channel > 1) return;
    drive_a_2_state_t *s = state_for(tile);

    /* Update VOUT-GAIN-X in DAC-X-VOUT-CMP-CONFIG */
    uint16_t vout_cfg = dac_read(tile, dac_vout_cmp_reg(channel));
    vout_cfg &= ~(0x07 << 10);
    vout_cfg |= ((uint16_t)gain << 10);
    dac_write(tile, dac_vout_cmp_reg(channel), vout_cfg);

    /* Update EN-INT-REF in COMMON-CONFIG if needed */
    uint16_t common_cfg = dac_read(tile, DAC63202W_REG_COMMON_CONFIG);
    if (gain >= DRIVE_A_2_GAIN_1P5X_INT)
        common_cfg |= (1 << 12);
    else if (s->gain[1 - channel] < DRIVE_A_2_GAIN_1P5X_INT)
        common_cfg &= ~(1 << 12);  /* safe to disable if neither channel needs it */
    dac_write(tile, DAC63202W_REG_COMMON_CONFIG, common_cfg);

    /* Cache */
    s->gain[channel] = (uint8_t)gain;
    s->vref_mv = resolve_vref((uint8_t)gain);
}

/* ================================================================
 * Public API — Waveform generation
 * ================================================================ */

void tile_drive_a_2_set_waveform(tile_t *tile, uint8_t channel,
                                 drive_a_2_wave_t wave)
{
    if (channel > 1) return;
    uint16_t func_cfg = dac_read(tile, dac_func_cfg_reg(channel));
    func_cfg &= ~(0x07 << 8);                 /* clear FUNC-CONFIG-X */
    func_cfg |= ((uint16_t)wave << 8);         /* set waveform type */
    dac_write(tile, dac_func_cfg_reg(channel), func_cfg);
}

void tile_drive_a_2_start_waveform(tile_t *tile, uint8_t channel)
{
    if (channel > 1) return;
    /* START-FUNC-0 is bit 0, START-FUNC-1 is bit 12 */
    uint16_t trig = (channel == 0) ? 0x0001 : 0x1000;
    dac_write(tile, DAC63202W_REG_COMMON_DAC_TRIG, trig);
}

void tile_drive_a_2_stop_waveform(tile_t *tile, uint8_t channel)
{
    if (channel > 1) return;
    uint16_t func_cfg = dac_read(tile, dac_func_cfg_reg(channel));
    func_cfg &= ~(0x07 << 8);
    func_cfg |= ((uint16_t)DRIVE_A_2_WAVE_OFF << 8);
    dac_write(tile, dac_func_cfg_reg(channel), func_cfg);
}

/* ================================================================
 * Public API — Amplifier control
 * ================================================================ */

void tile_drive_a_2_amp_set_gain(tile_t *tile, int8_t gain_db)
{
    if (gain_db < -28) gain_db = -28;
    if (gain_db > 30) gain_db = 30;
    amp_write(tile, TPA2028D1_REG_AGC_GAIN, (uint8_t)(gain_db & 0x3F));
}

int8_t tile_drive_a_2_amp_get_gain(tile_t *tile)
{
    uint8_t raw = amp_read(tile, TPA2028D1_REG_AGC_GAIN) & 0x3F;
    /* Sign-extend 6-bit two's complement to int8_t */
    if (raw & 0x20) raw |= 0xC0;
    return (int8_t)raw;
}

void tile_drive_a_2_amp_enable(tile_t *tile)
{
    uint8_t reg1 = amp_read(tile, TPA2028D1_REG_FUNC_CTRL);
    reg1 |= (1 << 6);   /* EN = 1 */
    reg1 &= ~(1 << 5);  /* SWS = 0 */
    amp_write(tile, TPA2028D1_REG_FUNC_CTRL, reg1);
}

void tile_drive_a_2_amp_disable(tile_t *tile)
{
    uint8_t reg1 = amp_read(tile, TPA2028D1_REG_FUNC_CTRL);
    reg1 |= (1 << 5);   /* SWS = 1 (software shutdown) */
    amp_write(tile, TPA2028D1_REG_FUNC_CTRL, reg1);
}

void tile_drive_a_2_amp_set_agc(tile_t *tile, const drive_a_2_agc_cfg_t *cfg)
{
    if (!cfg) return;

    amp_write(tile, TPA2028D1_REG_AGC_ATTACK,
              cfg->attack & 0x3F);

    amp_write(tile, TPA2028D1_REG_AGC_RELEASE,
              cfg->release & 0x3F);

    amp_write(tile, TPA2028D1_REG_AGC_HOLD,
              cfg->hold & 0x3F);

    amp_write(tile, TPA2028D1_REG_AGC_GAIN,
              (uint8_t)(cfg->fixed_gain_db & 0x3F));

    /* Register 0x06: [7] limiter disable, [6:5] noise gate, [4:0] limiter level */
    uint8_t ctrl1 = (cfg->limiter_level & 0x1F)
                  | ((cfg->noise_gate & 0x03) << 5);
    if (cfg->compression == DRIVE_A_2_COMP_1_1)
        ctrl1 |= (1 << 7);  /* disable limiter when compression is off */
    amp_write(tile, TPA2028D1_REG_AGC_CTRL1, ctrl1);

    /* Register 0x07: [7:4] max gain (value - 18), [1:0] compression ratio */
    uint8_t max_gain = cfg->max_gain_db;
    if (max_gain < 18) max_gain = 18;
    if (max_gain > 30) max_gain = 30;
    uint8_t ctrl2 = ((max_gain - 18) << 4) | (cfg->compression & 0x03);
    amp_write(tile, TPA2028D1_REG_AGC_CTRL2, ctrl2);
}

uint8_t tile_drive_a_2_amp_read_status(tile_t *tile)
{
    return amp_read(tile, TPA2028D1_REG_FUNC_CTRL);
}

/* ================================================================
 * Public API — Status
 * ================================================================ */

uint16_t tile_drive_a_2_read_status(tile_t *tile)
{
    return dac_read(tile, DAC63202W_REG_GENERAL_STATUS);
}
