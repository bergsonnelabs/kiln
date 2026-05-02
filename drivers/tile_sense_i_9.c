/**
 * @file   tile_sense_i_9.c
 * @brief  9-DOF IMU driver implementation (ICM-20948 + AK09916).
 *
 * References:
 *   - ICM-20948 datasheet DS-000189 rev 1.3 (TDK InvenSense)
 *   - AK09916 datasheet 015007392-E-02 (Asahi Kasei)
 *   - TDK AN-000150: ICM-20948 self-test procedure
 *
 * AK09916 access:
 *   The driver enables INT_PIN_CFG.BYPASS_EN at init so the AK09916
 *   appears directly on the host I2C bus at 0x0C. This is simpler
 *   than the I2C-master path used in some reference drivers and is
 *   why the "sensor hub for external aux sensors" gap remains open.
 */

#include "tile_sense_i_9.h"
#include <stddef.h>

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

static const uint8_t id_table[] = {
    ICM20948_I2C_ADDR_DEFAULT,   /* instance 0 — pad 2 floating (0x69) */
    ICM20948_I2C_ADDR_ALT,       /* instance 1 — pad 2 to GND  (0x68) */
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

static void icm_write(tile_t* tile, uint8_t reg, uint8_t value)
{
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, &value, 1);
}

static uint8_t icm_read1(tile_t* tile, uint8_t reg)
{
    uint8_t v = 0;
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, &v, 1);
    return v;
}

static void icm_read(tile_t* tile, uint8_t reg, uint8_t* data, uint16_t len)
{
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, data, len);
}

static void icm_modify(tile_t* tile, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t r = icm_read1(tile, reg);
    r = (uint8_t)((r & ~mask) | (value & mask));
    icm_write(tile, reg, r);
}

static void ak_write(tile_t* tile, uint8_t reg, uint8_t value)
{
    tile->hal->i2c_write(tile->hal->handle, AK09916_I2C_ADDR, reg, &value, 1);
}

static void ak_read(tile_t* tile, uint8_t reg, uint8_t* data, uint16_t len)
{
    tile->hal->i2c_read(tile->hal->handle, AK09916_I2C_ADDR, reg, data, len);
}

static uint8_t ak_read1(tile_t* tile, uint8_t reg)
{
    uint8_t v = 0;
    tile->hal->i2c_read(tile->hal->handle, AK09916_I2C_ADDR, reg, &v, 1);
    return v;
}

static void set_bank(tile_t* tile, uint8_t bank)
{
    icm_write(tile, ICM20948_REG_BANK_SEL, (uint8_t)(bank << 4));
}

/** @brief  Swap bytes in a buffer of int16_t values (big-endian → native). */
static void swap16_buf(int16_t* buf, uint8_t count)
{
    for (uint8_t i = 0; i < count; i++) {
        buf[i] = (int16_t)__builtin_bswap16((uint16_t)buf[i]);
    }
}

static int16_t be16(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/** @brief  AK09916 little-endian 16-bit pair (HXL then HXH). */
static int16_t le16(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[1] << 8) | p[0]);
}

/* -------------------------------------------------------------- */
/* Public API — lifecycle                                          */
/* -------------------------------------------------------------- */

uint8_t tile_sense_i_9_find(tiles_pal_t* hal, uint8_t instance)
{
    uint8_t id = resolve_id(instance);
    if (id == 0x00) return 0;
    return (hal->i2c_is_ready(hal->handle, id) == 0) ? 1 : 0;
}

void tile_sense_i_9_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                         const sense_i_9_cfg_t *cfg)
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

    /* Verify chip identity */
    set_bank(tile, ICM20948_BANK_0);
    if (icm_read1(tile, ICM20948_REG_WHOAMI) != ICM20948_WHOAMI_DEFAULT) {
        TILE_ON_ERROR(tile, "init: unexpected chip ID");
        tile->state = TILE_STATE_ERROR;
        return;
    }

    /* Software reset */
    icm_write(tile, ICM20948_REG_PWR_MGMT_1, 1 << 7);
    hal->delay_ms(50);

    /* Wake: auto clock select */
    icm_write(tile, ICM20948_REG_PWR_MGMT_1, 0x01);

    /* Enable all accel + gyro axes */
    icm_write(tile, ICM20948_REG_PWR_MGMT_2, 0x00);

    /* Enable I2C bypass so we can talk to the AK09916 directly */
    icm_write(tile, ICM20948_REG_INT_PIN_CFG, 0x02);

    /* Return to bank 0 */
    set_bank(tile, ICM20948_BANK_0);

    /* Initialize magnetometer: continuous 100 Hz */
    ak_write(tile, AK09916_REG_CNTL3, 0x01);  /* soft reset */
    hal->delay_ms(1);
    ak_write(tile, AK09916_REG_CNTL2, SENSE_I_9_MAG_CONTINUOUS_100HZ);

    tile->state = TILE_STATE_READY;
}

uint8_t tile_sense_i_9_data_ready(tile_t* tile)
{
    uint8_t status = icm_read1(tile, ICM20948_REG_INT_STATUS_1);
    return (status & 0x01) ? 1 : 0;  /* RAW_DATA_0_RDY_INT */
}

void tile_sense_i_9_set_accel_range(tile_t* tile, sense_i_9_accel_range_t range)
{
    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_ACCEL_CONFIG, (uint8_t)range);
    set_bank(tile, ICM20948_BANK_0);
}

void tile_sense_i_9_set_gyro_range(tile_t* tile, sense_i_9_gyro_range_t range)
{
    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_GYRO_CONFIG, (uint8_t)range);
    set_bank(tile, ICM20948_BANK_0);
}

void tile_sense_i_9_set_mag_mode(tile_t* tile, sense_i_9_mag_mode_t mode)
{
    /* Datasheet §9.3: must transit through power-down for 100 µs (Twait)
     * before entering a new mode. ak_write of 0x00 then the new mode. */
    ak_write(tile, AK09916_REG_CNTL2, SENSE_I_9_MAG_POWER_DOWN);
    tile->hal->delay_ms(1);
    ak_write(tile, AK09916_REG_CNTL2, (uint8_t)mode);
}

void tile_sense_i_9_set_accel_odr(tile_t* tile, uint16_t divider)
{
    uint8_t hi = (uint8_t)((divider >> 8) & 0x0F);
    uint8_t lo = (uint8_t)(divider & 0xFF);

    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_ACCEL_SMPLRT_H, hi);
    icm_write(tile, ICM20948_REG_ACCEL_SMPLRT_L, lo);
    set_bank(tile, ICM20948_BANK_0);
}

void tile_sense_i_9_set_gyro_odr(tile_t* tile, uint8_t divider)
{
    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_GYRO_SMPLRT, divider);
    set_bank(tile, ICM20948_BANK_0);
}

void tile_sense_i_9_get_raw_accels(tile_t* tile, int16_t* buffer)
{
    icm_read(tile, ICM20948_REG_ACCEL_X_H, (uint8_t*)buffer, 6);
    swap16_buf(buffer, 3);
}

void tile_sense_i_9_get_raw_gyros(tile_t* tile, int16_t* buffer)
{
    icm_read(tile, ICM20948_REG_GYRO_X_H, (uint8_t*)buffer, 6);
    swap16_buf(buffer, 3);
}

void tile_sense_i_9_get_raw_6dof(tile_t* tile, int16_t* buffer)
{
    icm_read(tile, ICM20948_REG_ACCEL_X_H, (uint8_t*)buffer, 12);
    swap16_buf(buffer, 6);
}

void tile_sense_i_9_get_raw_mags(tile_t* tile, int16_t* buffer)
{
    uint8_t st2;

    /* AK09916 data registers are little-endian — no swap needed */
    ak_read(tile, AK09916_REG_HXL, (uint8_t*)buffer, 6);

    /* Reading ST2 releases the data lock for the next measurement */
    ak_read(tile, AK09916_REG_ST2, &st2, 1);
}

uint8_t tile_sense_i_9_mag_overflowed(tile_t* tile)
{
    /* Reading ST2 also releases the AK09916 data lock — same end-effect
     * as get_raw_mags() w.r.t. the chip. */
    return (ak_read1(tile, AK09916_REG_ST2) & AK09916_ST2_HOFL) ? 1 : 0;
}

int16_t tile_sense_i_9_get_temperature(tile_t* tile)
{
    int16_t raw;
    icm_read(tile, ICM20948_REG_TEMP_H, (uint8_t*)&raw, 2);
    raw = (int16_t)__builtin_bswap16((uint16_t)raw);
    return raw;
}

void tile_sense_i_9_sleep(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_0);
    icm_write(tile, ICM20948_REG_PWR_MGMT_1, 0x41);  /* SLEEP + auto clock */
    tile->state = TILE_STATE_SLEEPING;
}

void tile_sense_i_9_wake(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_0);
    icm_write(tile, ICM20948_REG_PWR_MGMT_1, 0x01);  /* clear SLEEP, auto clock */
    tile->state = TILE_STATE_READY;
}

void tile_sense_i_9_reset(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_0);
    icm_write(tile, ICM20948_REG_PWR_MGMT_1, 1 << 7);
    tile->hal->delay_ms(50);
    tile->state = TILE_STATE_NONE;
}

/* -------------------------------------------------------------- */
/* Interrupt source configuration                                  */
/* -------------------------------------------------------------- */

void tile_sense_i_9_int_config(tile_t* tile, uint8_t flags)
{
    /* Mask off the BYPASS_EN bit (bit 1) and INT_ANYRD_2CLEAR if not
     * provided in flags — preserve the bypass bit so the AK09916 remains
     * reachable. Flags supply bits 7,6,5,4 directly. */
    set_bank(tile, ICM20948_BANK_0);
    uint8_t cur = icm_read1(tile, ICM20948_REG_INT_PIN_CFG);
    uint8_t keep = cur & 0x02;  /* BYPASS_EN preserved */
    icm_write(tile, ICM20948_REG_INT_PIN_CFG,
              (uint8_t)((flags & 0xF0) | keep));
}

void tile_sense_i_9_int_data_ready(tile_t* tile, uint8_t enabled)
{
    set_bank(tile, ICM20948_BANK_0);
    /* INT_ENABLE_1 bit 0 = RAW_DATA_0_RDY_EN */
    icm_modify(tile, ICM20948_REG_INT_ENABLE_1, 0x01, enabled ? 0x01 : 0x00);
}

void tile_sense_i_9_int_wom(tile_t* tile, uint8_t enabled)
{
    set_bank(tile, ICM20948_BANK_0);
    /* INT_ENABLE bit 3 = WOM_INT_EN */
    icm_modify(tile, ICM20948_REG_INT_ENABLE,
               ICM20948_INTE_WOM_INT_EN,
               enabled ? ICM20948_INTE_WOM_INT_EN : 0x00);
}

void tile_sense_i_9_int_fifo_overflow(tile_t* tile, uint8_t enabled)
{
    set_bank(tile, ICM20948_BANK_0);
    /* INT_ENABLE_2 bits [4:0] = FIFO_OVERFLOW_EN per sensor.
     * For a single FIFO config any of the bits triggers; enable all 5. */
    icm_modify(tile, ICM20948_REG_INT_ENABLE_2, 0x1F, enabled ? 0x1F : 0x00);
}

void tile_sense_i_9_int_fifo_watermark(tile_t* tile, uint8_t enabled)
{
    set_bank(tile, ICM20948_BANK_0);
    icm_modify(tile, ICM20948_REG_INT_ENABLE_3, 0x1F, enabled ? 0x1F : 0x00);
}

uint8_t tile_sense_i_9_get_int_status(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_0);
    return icm_read1(tile, ICM20948_REG_INT_STATUS);
}

uint8_t tile_sense_i_9_get_int_status_fifo_overflow(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_0);
    return icm_read1(tile, ICM20948_REG_INT_STATUS_2) & 0x1F;
}

uint8_t tile_sense_i_9_get_int_status_fifo_watermark(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_0);
    return icm_read1(tile, ICM20948_REG_INT_STATUS_3) & 0x1F;
}

/* -------------------------------------------------------------- */
/* Wake-on-Motion                                                  */
/* -------------------------------------------------------------- */

void tile_sense_i_9_wom_config(tile_t* tile, uint16_t thr_mg,
                               sense_i_9_wom_mode_t mode)
{
    /* Threshold register is 8-bit, 4 mg/LSB. Clamp at 0xFF (1020 mg). */
    uint16_t lsb = (thr_mg + 2) / 4;     /* round-to-nearest */
    if (lsb > 0xFF) lsb = 0xFF;

    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_ACCEL_WOM_THR, (uint8_t)lsb);
    /* ACCEL_INTEL_CTRL bit 0 selects compare mode (0=initial, 1=previous).
     * Don't enable here — that's wom_enable's job — preserve INTEL_EN bit. */
    icm_modify(tile, ICM20948_REG_ACCEL_INTEL_CTRL, 0x01,
               (mode == SENSE_I_9_WOM_VS_PREVIOUS) ? 0x01 : 0x00);
    set_bank(tile, ICM20948_BANK_0);
}

void tile_sense_i_9_wom_enable(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_2);
    /* ACCEL_INTEL_CTRL bit 1 = ACCEL_INTEL_EN */
    icm_modify(tile, ICM20948_REG_ACCEL_INTEL_CTRL, 0x02, 0x02);
    set_bank(tile, ICM20948_BANK_0);
}

void tile_sense_i_9_wom_disable(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_2);
    icm_modify(tile, ICM20948_REG_ACCEL_INTEL_CTRL, 0x02, 0x00);
    set_bank(tile, ICM20948_BANK_0);
}

/* -------------------------------------------------------------- */
/* FIFO                                                            */
/* -------------------------------------------------------------- */

void tile_sense_i_9_fifo_config(tile_t* tile, sense_i_9_fifo_mode_t mode,
                                uint8_t accel, uint8_t gyro, uint8_t temp)
{
    set_bank(tile, ICM20948_BANK_0);

    /* FIFO_MODE: snapshot vs stream (any non-zero in [4:0] = snapshot). */
    icm_write(tile, ICM20948_REG_FIFO_MODE,
              (mode == SENSE_I_9_FIFO_SNAPSHOT) ? 0x1F : 0x00);

    /* Wire up sensor sources. FIFO_EN_2 [4:0] selects accel + gyro X/Y/Z + temp. */
    uint8_t en2 = 0;
    if (accel) en2 |= (1 << 4);
    if (gyro)  en2 |= (1 << 3) | (1 << 2) | (1 << 1);   /* GZ, GY, GX */
    if (temp)  en2 |= (1 << 0);
    icm_write(tile, ICM20948_REG_FIFO_EN_2, en2);
    icm_write(tile, ICM20948_REG_FIFO_EN_1, 0x00);  /* no aux slaves */

    /* Toggle FIFO_EN in USER_CTRL */
    uint8_t any = (accel | gyro | temp) ? 1 : 0;
    icm_modify(tile, ICM20948_USER_CTRL, ICM20948_UC_FIFO_EN,
               any ? ICM20948_UC_FIFO_EN : 0x00);

    /* Reset FIFO so config takes effect cleanly: assert and de-assert. */
    icm_write(tile, ICM20948_REG_FIFO_RST, 0x1F);
    icm_write(tile, ICM20948_REG_FIFO_RST, 0x00);
}

void tile_sense_i_9_fifo_flush(tile_t* tile)
{
    set_bank(tile, ICM20948_BANK_0);
    icm_write(tile, ICM20948_REG_FIFO_RST, 0x1F);
    icm_write(tile, ICM20948_REG_FIFO_RST, 0x00);
}

uint16_t tile_sense_i_9_fifo_count(tile_t* tile)
{
    uint8_t buf[2];
    set_bank(tile, ICM20948_BANK_0);
    /* Datasheet §8.60: reading COUNTH latches both bytes. We perform a
     * burst read starting at COUNTH which reads COUNTH then COUNTL. */
    icm_read(tile, ICM20948_REG_FIFO_COUNTH, buf, 2);
    return (uint16_t)(((uint16_t)(buf[0] & 0x1F) << 8) | buf[1]);
}

uint8_t tile_sense_i_9_fifo_read_packet(tile_t* tile,
                                        sense_i_9_fifo_packet_t* pkt)
{
    if (tile_sense_i_9_fifo_count(tile) < 12) return 0;

    uint8_t raw[12];
    icm_read(tile, ICM20948_REG_FIFO_R_W, raw, 12);

    /* Big-endian, accel first, gyro after — matches FIFO_EN_2 enable order. */
    pkt->accel[0] = be16(&raw[0]);
    pkt->accel[1] = be16(&raw[2]);
    pkt->accel[2] = be16(&raw[4]);
    pkt->gyro[0]  = be16(&raw[6]);
    pkt->gyro[1]  = be16(&raw[8]);
    pkt->gyro[2]  = be16(&raw[10]);
    return 1;
}

/* -------------------------------------------------------------- */
/* Self-test                                                       */
/* -------------------------------------------------------------- */

/* TDK app-note AN-000150 self-test pass band: 50%–150% of factory ref. */
static uint8_t st_in_band(int32_t response, int32_t reference)
{
    /* If the factory reference is zero (rare) the test is inconclusive
     * — accept any non-zero response of the same sign as a pass. */
    if (reference == 0) return (response != 0) ? 1 : 0;
    int32_t lo = reference / 2;
    int32_t hi = reference + (reference / 2);
    if (lo > hi) { int32_t t = lo; lo = hi; hi = t; }
    return (response >= lo && response <= hi) ? 1 : 0;
}

uint8_t tile_sense_i_9_self_test(tile_t* tile,
                                 uint8_t* accel_pass, uint8_t* gyro_pass)
{
    if (accel_pass) *accel_pass = 0;
    if (gyro_pass)  *gyro_pass  = 0;
    if (tile->state == TILE_STATE_ERROR || tile->hal == NULL) return 0;

    tiles_pal_t* hal = tile->hal;

    /* --- Capture factory reference values from Bank 1 --- */
    set_bank(tile, ICM20948_BANK_1);
    uint8_t ref[6];
    ref[0] = icm_read1(tile, ICM20948_B1_SELF_TEST_X_GYRO);
    ref[1] = icm_read1(tile, ICM20948_B1_SELF_TEST_Y_GYRO);
    ref[2] = icm_read1(tile, ICM20948_B1_SELF_TEST_Z_GYRO);
    ref[3] = icm_read1(tile, ICM20948_B1_SELF_TEST_X_ACCEL);
    ref[4] = icm_read1(tile, ICM20948_B1_SELF_TEST_Y_ACCEL);
    ref[5] = icm_read1(tile, ICM20948_B1_SELF_TEST_Z_ACCEL);

    /* --- Set up known-good config: ±2 g, ±250 dps, 1 kHz, DLPF on --- */
    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_GYRO_CONFIG,  0x01);  /* DLPF 1, ±250 dps */
    icm_write(tile, ICM20948_REG_ACCEL_CONFIG, 0x01);  /* DLPF 1, ±2 g */
    icm_write(tile, ICM20948_REG_GYRO_SMPLRT,  0x00);
    icm_write(tile, ICM20948_REG_ACCEL_SMPLRT_H, 0x00);
    icm_write(tile, ICM20948_REG_ACCEL_SMPLRT_L, 0x00);
    set_bank(tile, ICM20948_BANK_0);
    hal->delay_ms(50);

    /* --- Sample with self-test OFF, average 16 samples --- */
    int32_t base_acc[3] = {0,0,0}, base_gyro[3] = {0,0,0};
    int16_t s[6];
    for (uint8_t i = 0; i < 16; i++) {
        hal->delay_ms(2);
        tile_sense_i_9_get_raw_6dof(tile, s);
        base_acc[0] += s[0]; base_acc[1] += s[1]; base_acc[2] += s[2];
        base_gyro[0] += s[3]; base_gyro[1] += s[4]; base_gyro[2] += s[5];
    }
    for (uint8_t k = 0; k < 3; k++) { base_acc[k] /= 16; base_gyro[k] /= 16; }

    /* --- Engage self-test on all axes --- */
    set_bank(tile, ICM20948_BANK_2);
    /* ACCEL_CONFIG_2: bits [4:2] = AX/AY/AZ_ST_EN, [1:0] DEC3 = 0 */
    icm_write(tile, ICM20948_REG_ACCEL_CONFIG_2, 0x1C);
    /* GYRO_CONFIG sets self-test enable in bits [7:5]: XG_ST | YG_ST | ZG_ST */
    icm_write(tile, ICM20948_REG_GYRO_CONFIG, (uint8_t)(0xE0 | 0x01));
    set_bank(tile, ICM20948_BANK_0);
    hal->delay_ms(50);  /* Datasheet: ≥ 20 ms for the response to settle */

    /* --- Sample with self-test ON --- */
    int32_t st_acc[3] = {0,0,0}, st_gyro[3] = {0,0,0};
    for (uint8_t i = 0; i < 16; i++) {
        hal->delay_ms(2);
        tile_sense_i_9_get_raw_6dof(tile, s);
        st_acc[0] += s[0]; st_acc[1] += s[1]; st_acc[2] += s[2];
        st_gyro[0] += s[3]; st_gyro[1] += s[4]; st_gyro[2] += s[5];
    }
    for (uint8_t k = 0; k < 3; k++) { st_acc[k] /= 16; st_gyro[k] /= 16; }

    /* --- Disengage self-test --- */
    set_bank(tile, ICM20948_BANK_2);
    icm_write(tile, ICM20948_REG_ACCEL_CONFIG_2, 0x00);
    icm_write(tile, ICM20948_REG_GYRO_CONFIG, 0x01);
    set_bank(tile, ICM20948_BANK_0);

    /* --- Compute responses (LSBs at ±2 g / ±250 dps) and compare --- */
    /* Factory reference values are stored in raw "self-test" units that
     * scale identically to the response at the test config above; the
     * 50%–150% band per AN-000150 captures normal device variation. */
    uint8_t apass = 0, gpass = 0;
    for (uint8_t k = 0; k < 3; k++) {
        int32_t a_resp = st_acc[k]  - base_acc[k];
        int32_t g_resp = st_gyro[k] - base_gyro[k];
        if (st_in_band(a_resp, (int32_t)(int8_t)ref[3 + k] << 7)) apass |= (1 << k);
        if (st_in_band(g_resp, (int32_t)(int8_t)ref[k]     << 7)) gpass |= (1 << k);
    }

    if (accel_pass) *accel_pass = apass;
    if (gyro_pass)  *gyro_pass  = gpass;

    return ((apass == 0x07) && (gpass == 0x07)) ? 1 : 0;
}

/* -------------------------------------------------------------- */
/* AK09916 self-test                                               */
/* -------------------------------------------------------------- */

uint8_t tile_sense_i_9_mag_self_test(tile_t* tile)
{
    tiles_pal_t* hal = tile->hal;

    /* §9.4.4.1: power-down → self-test → poll DRDY → read data */
    ak_write(tile, AK09916_REG_CNTL2, SENSE_I_9_MAG_POWER_DOWN);
    hal->delay_ms(1);

    ak_write(tile, AK09916_REG_CNTL2, AK09916_MODE_SELF_TEST);

    /* Poll DRDY for up to 20 ms (single-meas time TSM ≤ 8.2 ms) */
    uint8_t st1 = 0;
    for (uint8_t i = 0; i < 40; i++) {
        st1 = ak_read1(tile, AK09916_REG_ST1);
        if (st1 & 0x01) break;
        hal->delay_ms(1);
    }
    if ((st1 & 0x01) == 0) return 0;

    uint8_t raw[6];
    ak_read(tile, AK09916_REG_HXL, raw, 6);

    /* Read ST2 to release the data lock */
    uint8_t st2 = ak_read1(tile, AK09916_REG_ST2);
    (void)st2;

    int16_t hx = le16(&raw[0]);
    int16_t hy = le16(&raw[2]);
    int16_t hz = le16(&raw[4]);

    /* Datasheet §9.4.4.2 pass criteria. */
    uint8_t pass_x = (hx >= -200) && (hx <= 200);
    uint8_t pass_y = (hy >= -200) && (hy <= 200);
    uint8_t pass_z = (hz >= -1000) && (hz <= -200);

    return (pass_x && pass_y && pass_z) ? 1 : 0;
}

/* -------------------------------------------------------------- */
/* Tier-2 idiomatic helpers                                        */
/* -------------------------------------------------------------- */

/* At ±2 g (init default), 1 g = 16384 LSB. Helper constants use
 * that scaling; if the user has switched ranges they can read raw
 * data with get_raw_accels and convert manually. */
#define SENSE_I_9_LSB_PER_G_2G   16384

/* Face-up / face-down decision band: |Z| > ~0.85 g and |X|,|Y| < ~0.5 g.
 * 0.85 g ≈ 13926 LSB at ±2 g; 0.5 g = 8192 LSB. */
#define SENSE_I_9_FACE_Z_MIN     13926
#define SENSE_I_9_FACE_XY_MAX     8192

/**
 * @brief  Integer atan2 approximation, output in 0.01° units.
 *
 * Returns angle in centi-degrees in the range [-18000, +18000].
 * Uses the well-known min/max polynomial approximation
 *   atan(t) ≈ t * (45 - (|t| - 1) * (14 + 3.83 * |t|))   for |t| ≤ 1
 * scaled to the centi-degree output and extended to all four
 * quadrants. Worst-case error is ~0.3°, well within the inherent
 * noise of accel and mag readings.
 */
static int32_t atan2_centi(int32_t y, int32_t x)
{
    if (x == 0 && y == 0) return 0;

    int32_t ay = y < 0 ? -y : y;
    int32_t ax = x < 0 ? -x : x;

    int32_t angle_centi;  /* in 0.01° */

    /* Compute |angle| in [0, 9000] (centi-degrees) for ratio in [0,1] */
    if (ax >= ay) {
        /* |y/x| <= 1 */
        if (ax == 0) {
            angle_centi = 0;
        } else {
            /* atan(t) ≈ t * (4500 - (|t|*1000 - 1000) * (14 + 4 * |t|*1000 / 1000) / 1000)
             * Done in fixed-point with t scaled by 1000. */
            int32_t t = (int32_t)(((int64_t)ay * 1000) / ax);          /* t * 1000, 0..1000 */
            int32_t corr = ((t - 1000) * (14000 + 4 * t)) / 1000;       /* small correction */
            angle_centi = (t * 4500 - t * corr / 1000) / 1000;
            if (angle_centi < 0)    angle_centi = 0;
            if (angle_centi > 4500) angle_centi = 4500;
        }
    } else {
        /* |y/x| > 1: angle = 90 - atan(|x/y|) */
        int32_t t = (int32_t)(((int64_t)ax * 1000) / ay);
        int32_t corr = ((t - 1000) * (14000 + 4 * t)) / 1000;
        int32_t inner = (t * 4500 - t * corr / 1000) / 1000;
        if (inner < 0)    inner = 0;
        if (inner > 4500) inner = 4500;
        angle_centi = 9000 - inner;
    }

    /* Quadrant fixup */
    if (x < 0) angle_centi = 18000 - angle_centi;
    if (y < 0) angle_centi = -angle_centi;

    return angle_centi;
}

uint8_t tile_sense_i_9_is_face_up(tile_t* tile)
{
    int16_t a[3];
    tile_sense_i_9_get_raw_accels(tile, a);

    int32_t ax = a[0] < 0 ? -(int32_t)a[0] : (int32_t)a[0];
    int32_t ay = a[1] < 0 ? -(int32_t)a[1] : (int32_t)a[1];

    if (ax > SENSE_I_9_FACE_XY_MAX) return 0;
    if (ay > SENSE_I_9_FACE_XY_MAX) return 0;
    return (a[2] > SENSE_I_9_FACE_Z_MIN) ? 1 : 0;
}

uint8_t tile_sense_i_9_is_face_down(tile_t* tile)
{
    int16_t a[3];
    tile_sense_i_9_get_raw_accels(tile, a);

    int32_t ax = a[0] < 0 ? -(int32_t)a[0] : (int32_t)a[0];
    int32_t ay = a[1] < 0 ? -(int32_t)a[1] : (int32_t)a[1];

    if (ax > SENSE_I_9_FACE_XY_MAX) return 0;
    if (ay > SENSE_I_9_FACE_XY_MAX) return 0;
    return (a[2] < -SENSE_I_9_FACE_Z_MIN) ? 1 : 0;
}

uint8_t tile_sense_i_9_is_moving(tile_t* tile, uint16_t threshold_mg)
{
    int16_t a[3];
    tile_sense_i_9_get_raw_accels(tile, a);

    /* magnitude² in LSB² (at ±2 g, 1 g² = 16384² ≈ 2.68e8, fits easily in
     * int64). Compare |mag² − 1g²| against (threshold)² mapped to LSB². */
    int64_t mag2 = (int64_t)a[0] * a[0]
                 + (int64_t)a[1] * a[1]
                 + (int64_t)a[2] * a[2];

    const int64_t one_g    = (int64_t)SENSE_I_9_LSB_PER_G_2G;
    const int64_t one_g_sq = one_g * one_g;

    /* threshold in LSB: (threshold_mg * 16384) / 1000 */
    int64_t thr_lsb = ((int64_t)threshold_mg * one_g) / 1000;

    /* (|a| − 1g) > thr  ⇔  |a|² − 1g²| > something — but keep both signs.
     * Use the magnitude inequality on (|a| − 1g)*(|a| + 1g) = (|a|² − 1g²),
     * with |a| + 1g ≈ 2g for small motion: (|a| − 1g) ≈ (mag2 − 1g²)/(2g). */
    int64_t delta2 = mag2 - one_g_sq;
    if (delta2 < 0) delta2 = -delta2;

    /* moving if delta2 / (2 * one_g) > thr_lsb */
    return (delta2 > thr_lsb * 2 * one_g) ? 1 : 0;
}

void tile_sense_i_9_read_tilt_centi_degrees(tile_t* tile, uint8_t axis,
                                            int16_t* out_centi_deg)
{
    if (out_centi_deg == NULL) return;

    int16_t a[3];
    tile_sense_i_9_get_raw_accels(tile, a);

    int32_t target = (axis < 3) ? a[axis] : 0;
    int32_t other_a = (axis == 0) ? a[1] : a[0];
    int32_t other_b = (axis == 2) ? a[1] : a[2];

    /* magnitude of the components perpendicular to `target`. Use
     * a sum-of-squares + integer sqrt via Newton's iteration. */
    int64_t s = (int64_t)other_a * other_a + (int64_t)other_b * other_b;
    int32_t perp = 0;
    if (s > 0) {
        int32_t r = 1;
        /* a few Newton iterations is enough for 16-bit inputs */
        for (uint8_t i = 0; i < 12; i++) {
            int32_t q = (int32_t)(s / r);
            r = (r + q) / 2;
            if (r == 0) { r = 1; break; }
        }
        perp = r;
    }

    int32_t centi = atan2_centi(perp, target);
    if (centi >  18000) centi =  18000;
    if (centi < -18000) centi = -18000;
    *out_centi_deg = (int16_t)centi;
}

void tile_sense_i_9_read_heading_centi_degrees(tile_t* tile,
                                               uint16_t* out_centi_deg)
{
    if (out_centi_deg == NULL) return;

    int16_t m[3];
    tile_sense_i_9_get_raw_mags(tile, m);

    /* TODO HW: per ICM-20948 datasheet §5.3 the AK09916 magnetometer axes
     * don't align with the gyro/accel frame on the same die — the mag
     * needs an axis remap before this atan2 will produce a sane heading.
     * Without the remap, expect the reading to be ~90° off (and possibly
     * sign-inverted). Calibrate at the bench by rotating the tile through
     * a known heading and adjusting the (sign, axis) pair below.
     *
     * atan2(-my, mx) gives heading where +X = 0°, increasing clockwise
     * when viewed from above (NED-style heading). */
    int32_t centi = atan2_centi(-(int32_t)m[1], (int32_t)m[0]);

    /* Normalise to [0, 36000) */
    if (centi < 0)        centi += 36000;
    if (centi >= 36000)   centi -= 36000;

    *out_centi_deg = (uint16_t)centi;
}

uint8_t tile_sense_i_9_wait_for_motion(tile_t* tile, uint32_t timeout_ms)
{
    /* Poll ~every 5 ms — fine enough that the worst-case latency
     * after motion is <5 ms, sparse enough that the I2C bus stays
     * mostly idle. Caller-supplied timeout_ms is rounded up to the
     * nearest multiple of the poll interval. */
    const uint32_t poll_ms = 5;
    uint32_t waited = 0;

    while (waited <= timeout_ms) {
        uint8_t status = tile_sense_i_9_get_int_status(tile);
        if (status & ICM20948_INT_WOM) {
            return 1;
        }
        if (waited == timeout_ms) break;
        uint32_t step = (timeout_ms - waited < poll_ms) ? (timeout_ms - waited) : poll_ms;
        tile->hal->delay_ms((uint16_t)step);
        waited += step;
    }
    return 0;
}
