/**
 * @file   tile_sense_mic.c
 * @brief  Sense.MIC (MAX11645 + AMM-2742) — complete driver implementation.
 *
 * Platform-agnostic. All bus access via tile->hal raw I2C function pointers.
 * The MAX11645 is a command-based device (no register addresses); we use
 * i2c_write_raw / i2c_read_raw exclusively.
 *
 * Reference: Maxim MAX11644/MAX11645 datasheet, 19-4544; Rev 3, 9/09
 */

#include "tile_sense_mic.h"
#include <stddef.h>

/* ================================================================
 * Instance → I2C address table
 * ================================================================ */

static const uint8_t id_table[] = {
    MAX11645_I2C_ADDR,   /* 0: 0x36 (fixed address — only instance) */
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
    uint8_t  setup_byte;    /* Cached setup byte (Vref, clock, polarity) */
    uint8_t  config_byte;   /* Cached config byte (scan, channel, SGL/DIF) */
    uint16_t vref_mv;       /* Reference voltage in mV (for conversions) */
    uint16_t dc_offset;     /* Measured DC bias point (auto-calibrated at init) */
} mic_state_t;

static mic_state_t mic_state[NUM_INSTANCES];

static mic_state_t *state_for(tile_t *tile)
{
    for (uint8_t i = 0; i < NUM_INSTANCES; i++)
        if (id_table[i] == tile->id) return &mic_state[i];
    return &mic_state[0];
}

/* ================================================================
 * Private helpers — raw I2C access (no register addresses)
 *
 * The MAX11645 uses a command-based protocol:
 *   Write: [ADDR+W] [command byte]
 *   Read:  [ADDR+R] [data_hi] [data_lo]
 *
 * All writes use i2c_write_raw. All reads use i2c_read_raw.
 * ================================================================ */

static void mic_write_cmd(tile_t *tile, uint8_t cmd)
{
    tile->hal->i2c_write_raw(tile->hal->handle, tile->id, &cmd, 1);
}

static uint16_t mic_read_sample(tile_t *tile)
{
    uint8_t buf[2] = {0, 0};
    tile->hal->i2c_read_raw(tile->hal->handle, tile->id, buf, 2);
    /* 12-bit result: upper nibble of buf[0] is status/padding,
     * lower nibble is D11:D8, buf[1] is D7:D0 */
    return (uint16_t)(((buf[0] & 0x0F) << 8) | buf[1]);
}

static void memzero(void *p, uint8_t n)
{
    uint8_t *b = (uint8_t *)p;
    while (n--) *b++ = 0;
}

/* Build a setup byte from ref enum */
static uint8_t build_setup(uint8_t ref_sel)
{
    return MAX11645_SETUP_REG
         | ((uint8_t)ref_sel << 4)
         | MAX11645_CLK_INTERNAL
         | MAX11645_UNI
         | MAX11645_RST_NORESET;
}

/* Build a config byte from scan mode + channel */
static uint8_t build_config(uint8_t scan, uint8_t channel)
{
    return MAX11645_CONFIG_REG
         | ((uint8_t)scan << 5)
         | ((uint8_t)channel << 1)
         | MAX11645_SINGLE_ENDED;
}

/* Determine Vref in mV from ref enum, with user override */
static uint16_t resolve_vref(uint8_t ref_sel, uint16_t user_vref_mv)
{
    if (user_vref_mv) return user_vref_mv;
    switch (ref_sel) {
        case SENSE_MIC_REF_INTERNAL:
        case SENSE_MIC_REF_INTERNAL_BUF:
            return 2048;
        default:
            return 3300;  /* VDD or external — assume 3.3V */
    }
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

/** @brief Check if a Sense.MIC is present on the bus. */
uint8_t tile_sense_mic_find(tiles_hal_t *hal, uint8_t instance)
{
    uint8_t addr = resolve_id(instance);
    if (!addr) return 0;
    return hal->i2c_is_ready(hal->handle, addr) == 0;
}

/** @brief Initialize the MAX11645 ADC. */
void tile_sense_mic_init(tiles_hal_t *hal, uint8_t instance,
                         tile_t *tile, const sense_mic_cfg_t *cfg)
{
    memzero(tile, sizeof(tile_t));
    tile->hal = hal;
    tile->id  = resolve_id(instance);

    if (!tile->id) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_mic: invalid instance");
        return;
    }

    /* Probe bus */
    if (hal->i2c_is_ready(hal->handle, tile->id) != 0) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_mic: device not found at 0x36");
        return;
    }

    /* Initialize per-instance state */
    mic_state_t *s = state_for(tile);
    memzero(s, sizeof(mic_state_t));

    /* Apply config or defaults */
    uint8_t ref_sel = (cfg && cfg->ref) ? cfg->ref : SENSE_MIC_REF_VDD;
    uint8_t channel = (cfg && cfg->channel) ? cfg->channel : SENSE_MIC_CH_AIN0;
    uint8_t scan    = (cfg) ? cfg->scan : 0;  /* 0 = SCAN_UP (scan from AIN0) */

    s->setup_byte  = build_setup(ref_sel);
    s->config_byte = build_config(scan, channel);
    s->vref_mv     = resolve_vref(ref_sel, cfg ? cfg->vref_mv : 0);

    /* Send setup byte — configures Vref, clock, polarity */
    mic_write_cmd(tile, s->setup_byte);
    hal->delay_ms(1);  /* Settling time for reference */

    /* Send configuration byte — scan mode, channel, single-ended */
    mic_write_cmd(tile, s->config_byte);

    /* Verify the device responds by reading a sample.
     * The MAX11645 has no WHO_AM_I, so we check that we get a
     * plausible 12-bit value (not 0xFFFF which indicates bus error). */
    uint16_t test = mic_read_sample(tile);
    if (test > MAX11645_ADC_MAX) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_mic: test read failed");
        return;
    }

    /* Auto-calibrate DC offset: average 64 samples to measure the mic
     * bias point. This takes ~3.5 ms at 400 kHz I2C and captures the
     * actual quiescent voltage, which varies with supply and PCB layout
     * (typically 600–900 counts with VDD ref, not necessarily mid-scale). */
    {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < 64; i++)
            sum += mic_read_sample(tile);
        s->dc_offset = (uint16_t)(sum >> 6);
    }

    tile->state = TILE_STATE_READY;
}

/** @brief Enter low-power mode. */
void tile_sense_mic_sleep(tile_t *tile)
{
    /* Write a setup byte with RST=0 to reset the config register,
     * which puts the ADC into its lowest-power idle state.
     * Keep the reference selection to allow quick wake. */
    mic_state_t *s = state_for(tile);
    uint8_t sleep_setup = (s->setup_byte & ~MAX11645_RST_NORESET);  /* RST=0 */
    mic_write_cmd(tile, sleep_setup);
    tile->state = TILE_STATE_SLEEPING;
}

/** @brief Wake from sleep, restore previous configuration. */
void tile_sense_mic_wake(tile_t *tile)
{
    mic_state_t *s = state_for(tile);

    /* Re-send setup byte (with RST=1 to preserve config) */
    mic_write_cmd(tile, s->setup_byte);
    tile->hal->delay_ms(1);  /* Reference settling */

    /* Re-send configuration byte */
    mic_write_cmd(tile, s->config_byte);

    tile->state = TILE_STATE_READY;
}

/** @brief Reset config register to power-on defaults. Must call init() again. */
void tile_sense_mic_reset(tile_t *tile)
{
    /* Setup byte with RST=0 resets the configuration register */
    uint8_t reset_setup = MAX11645_SETUP_REG
                        | MAX11645_SEL_VDD
                        | MAX11645_CLK_INTERNAL
                        | MAX11645_UNI
                        | MAX11645_RST_RESET;
    mic_write_cmd(tile, reset_setup);
    tile->state = TILE_STATE_NONE;
}

/* ================================================================
 * Configuration
 * ================================================================ */

/** @brief Change the reference voltage source. */
void tile_sense_mic_set_reference(tile_t *tile, sense_mic_ref_t ref)
{
    mic_state_t *s = state_for(tile);

    /* Rebuild setup byte with new reference */
    s->setup_byte = build_setup((uint8_t)ref);
    s->vref_mv = resolve_vref((uint8_t)ref, 0);

    mic_write_cmd(tile, s->setup_byte);
    tile->hal->delay_ms(1);  /* Reference settling time */
}

/** @brief Change the active ADC channel. */
void tile_sense_mic_set_channel(tile_t *tile, sense_mic_channel_t ch)
{
    mic_state_t *s = state_for(tile);

    /* Rebuild config byte preserving scan mode */
    uint8_t scan = (s->config_byte >> 5) & 0x03;
    s->config_byte = build_config(scan, (uint8_t)ch);

    mic_write_cmd(tile, s->config_byte);
}

/** @brief Change the scan mode. */
void tile_sense_mic_set_scan_mode(tile_t *tile, sense_mic_scan_t scan)
{
    mic_state_t *s = state_for(tile);

    /* Rebuild config byte preserving channel */
    uint8_t channel = (s->config_byte >> 1) & 0x01;
    s->config_byte = build_config((uint8_t)scan, channel);

    mic_write_cmd(tile, s->config_byte);
}

/** @brief Get the currently configured reference voltage in millivolts. */
uint16_t tile_sense_mic_get_vref_mv(tile_t *tile)
{
    mic_state_t *s = state_for(tile);
    return s->vref_mv;
}

/* ================================================================
 * Data reads
 * ================================================================ */

/** @brief Read a single 12-bit ADC sample (0–4095). */
uint16_t tile_sense_mic_get_raw(tile_t *tile)
{
    return mic_read_sample(tile);
}

/** @brief Read a single sample and convert to millivolts. */
uint16_t tile_sense_mic_get_raw_mv(tile_t *tile)
{
    mic_state_t *s = state_for(tile);
    uint16_t raw = mic_read_sample(tile);
    /* mv = raw * vref_mv / 4096 — use 32-bit intermediate to avoid overflow */
    return (uint16_t)(((uint32_t)raw * s->vref_mv) >> 12);
}

/** @brief Read a single AC-coupled audio sample (signed, relative to DC offset). */
int16_t tile_sense_mic_get_audio_sample(tile_t *tile)
{
    mic_state_t *s = state_for(tile);
    uint16_t raw = mic_read_sample(tile);
    return (int16_t)raw - (int16_t)s->dc_offset;
}

/** @brief Get the auto-calibrated DC offset. */
uint16_t tile_sense_mic_get_dc_offset(tile_t *tile)
{
    mic_state_t *s = state_for(tile);
    return s->dc_offset;
}

/** @brief Re-calibrate the DC offset by averaging 64 samples. */
void tile_sense_mic_calibrate(tile_t *tile)
{
    mic_state_t *s = state_for(tile);
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 64; i++)
        sum += mic_read_sample(tile);
    s->dc_offset = (uint16_t)(sum >> 6);
}

/** @brief Burst-read N samples into a buffer. */
void tile_sense_mic_get_samples(tile_t *tile, uint16_t *buf, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++) {
        buf[i] = mic_read_sample(tile);
    }
}

/* ================================================================
 * Audio analysis utilities — pure computation, no I2C
 * ================================================================ */

/** @brief Compute the DC level (mean) of a sample buffer. */
uint16_t tile_sense_mic_dc_level(const uint16_t *samples, uint16_t count)
{
    if (count == 0) return 0;
    uint32_t sum = 0;
    for (uint16_t i = 0; i < count; i++) {
        sum += samples[i];
    }
    return (uint16_t)(sum / count);
}

/** @brief Compute peak-to-peak amplitude of a sample buffer. */
uint16_t tile_sense_mic_peak_to_peak(const uint16_t *samples, uint16_t count)
{
    if (count == 0) return 0;
    uint16_t min_val = samples[0];
    uint16_t max_val = samples[0];
    for (uint16_t i = 1; i < count; i++) {
        if (samples[i] < min_val) min_val = samples[i];
        if (samples[i] > max_val) max_val = samples[i];
    }
    return max_val - min_val;
}

/** @brief Compute RMS amplitude relative to a DC offset. */
uint16_t tile_sense_mic_rms(const uint16_t *samples, uint16_t count,
                            uint16_t dc_offset)
{
    if (count == 0) return 0;
    uint32_t sum_sq = 0;
    for (uint16_t i = 0; i < count; i++) {
        int32_t ac = (int32_t)samples[i] - (int32_t)dc_offset;
        sum_sq += (uint32_t)(ac * ac);
    }
    /* Integer square root — Newton's method */
    uint32_t mean_sq = sum_sq / count;
    if (mean_sq == 0) return 0;
    uint32_t x = mean_sq;
    uint32_t y = (x + 1) >> 1;
    while (y < x) {
        x = y;
        y = (x + mean_sq / x) >> 1;
    }
    return (uint16_t)x;
}

/** @brief Convert peak-to-peak raw amplitude to millivolts. */
uint16_t tile_sense_mic_amplitude_mv(tile_t *tile, uint16_t pp_raw)
{
    mic_state_t *s = state_for(tile);
    return (uint16_t)(((uint32_t)pp_raw * s->vref_mv) >> 12);
}
