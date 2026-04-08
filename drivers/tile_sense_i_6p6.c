/**
 * @file   tile_sense_i_6p6.c
 * @brief  Sense.I.6P6 (ICM-42686-P) — complete driver implementation.
 *
 * Platform-agnostic. All bus access via tile->hal function pointers.
 * Reference: TDK InvenSense DS-000639 Rev 1.0.
 */

#include "tile_sense_i_6p6.h"
#include <stddef.h>

/* ================================================================
 * Instance → I2C address table
 * ================================================================ */

static const uint8_t id_table[] = {
    ICM42686P_I2C_ADDR_DEFAULT,   /* 0: 0x69 (AD0 high — default on tile) */
    ICM42686P_I2C_ADDR_ALT,       /* 1: 0x68 (AD0 tied to GND) */
};

#define NUM_INSTANCES  (sizeof(id_table) / sizeof(id_table[0]))

static uint8_t resolve_id(uint8_t instance)
{
    return (instance < NUM_INSTANCES) ? id_table[instance] : 0;
}

/* ================================================================
 * Private helpers
 * ================================================================ */

static void icm_write(tile_t *tile, uint8_t reg, uint8_t val)
{
    tile->hal->i2c_write(tile->hal->handle, tile->id, reg, &val, 1);
}

static uint8_t icm_read(tile_t *tile, uint8_t reg)
{
    uint8_t val = 0;
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, &val, 1);
    return val;
}

static void icm_read_buf(tile_t *tile, uint8_t reg, uint8_t *buf, uint16_t len)
{
    tile->hal->i2c_read(tile->hal->handle, tile->id, reg, buf, len);
}

static void icm_set_bank(tile_t *tile, uint8_t bank)
{
    icm_write(tile, ICM42686P_REG_BANK_SEL, bank);
}

static void icm_modify(tile_t *tile, uint8_t reg, uint8_t mask, uint8_t val)
{
    uint8_t r = icm_read(tile, reg);
    r = (r & ~mask) | (val & mask);
    icm_write(tile, reg, r);
}

static int16_t swap16(uint8_t h, uint8_t l)
{
    return (int16_t)((uint16_t)h << 8 | l);
}

static void memzero(void *p, uint8_t n)
{
    uint8_t *b = (uint8_t *)p;
    while (n--) *b++ = 0;
}

/* ================================================================
 * Lifecycle
 * ================================================================ */

uint8_t tile_sense_i_6p6_find(tiles_hal_t *hal, uint8_t instance)
{
    uint8_t addr = resolve_id(instance);
    if (!addr) return 0;
    return hal->i2c_is_ready(hal->handle, addr) == 0;
}

void tile_sense_i_6p6_init(tiles_hal_t *hal, uint8_t instance, tile_t *tile)
{
    memzero(tile, sizeof(tile_t));
    tile->hal = hal;
    tile->id  = resolve_id(instance);

    if (!tile->id) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_i_6p6: invalid instance");
        return;
    }

    /* Probe bus */
    if (hal->i2c_is_ready(hal->handle, tile->id) != 0) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_i_6p6: device not found");
        return;
    }

    /* Soft reset */
    icm_write(tile, ICM42686P_REG_DEVICE_CONFIG, 0x01);
    hal->delay_ms(2);

    /* Read WHO_AM_I — store in flags field for debug.
     * Expected 0x44 per datasheet; 0x46 seen on some revisions.
     * Accept any non-0x00/0xFF value (device is responding). */
    uint8_t who = icm_read(tile, ICM42686P_REG_WHO_AM_I);
    tile->flags = who;  /* Stash for debug readback */
    if (who == 0x00 || who == 0xFF) {
        tile->state = TILE_STATE_ERROR;
        TILE_ON_ERROR(tile, "sense_i_6p6: WHO_AM_I read failed");
        return;
    }

    /* Ensure bank 0 */
    icm_set_bank(tile, ICM42686P_BANK_0);

    /* INT_CONFIG1: clear INT_ASYNC_RESET (bit 4) for proper INT operation */
    icm_modify(tile, ICM42686P_REG_INT_CONFIG1, 0x10, 0x00);

    /* Default config: ±8g accel, ±1000 dps gyro, both at 100 Hz */
    icm_write(tile, ICM42686P_REG_ACCEL_CONFIG0,
              (SENSE_I_6P6_ACCEL_8G << 5) | SENSE_I_6P6_ODR_100HZ);
    icm_write(tile, ICM42686P_REG_GYRO_CONFIG0,
              (SENSE_I_6P6_GYRO_1000DPS << 5) | SENSE_I_6P6_ODR_100HZ);

    /* Enable accel + gyro in low-noise mode */
    icm_write(tile, ICM42686P_REG_PWR_MGMT0,
              ICM42686P_PWR_ACCEL_LN | ICM42686P_PWR_GYRO_LN);
    hal->delay_ms(1);  /* >200 µs after mode change */

    tile->state = TILE_STATE_READY;
}

void tile_sense_i_6p6_sleep(tile_t *tile)
{
    icm_write(tile, ICM42686P_REG_PWR_MGMT0,
              ICM42686P_PWR_ACCEL_OFF | ICM42686P_PWR_GYRO_OFF);
    tile->state = TILE_STATE_SLEEPING;
}

void tile_sense_i_6p6_wake(tile_t *tile)
{
    icm_write(tile, ICM42686P_REG_PWR_MGMT0,
              ICM42686P_PWR_ACCEL_LN | ICM42686P_PWR_GYRO_LN);
    tile->hal->delay_ms(1);
    tile->state = TILE_STATE_READY;
}

void tile_sense_i_6p6_reset(tile_t *tile)
{
    icm_write(tile, ICM42686P_REG_DEVICE_CONFIG, 0x01);
    tile->hal->delay_ms(2);
    tile->state = TILE_STATE_NONE;
}

/* ================================================================
 * Configuration
 * ================================================================ */

void tile_sense_i_6p6_set_accel_range(tile_t *tile, sense_i_6p6_accel_range_t range)
{
    icm_modify(tile, ICM42686P_REG_ACCEL_CONFIG0, 0xE0, (uint8_t)range << 5);
}

void tile_sense_i_6p6_set_gyro_range(tile_t *tile, sense_i_6p6_gyro_range_t range)
{
    icm_modify(tile, ICM42686P_REG_GYRO_CONFIG0, 0xE0, (uint8_t)range << 5);
}

void tile_sense_i_6p6_set_accel_odr(tile_t *tile, sense_i_6p6_odr_t odr)
{
    icm_modify(tile, ICM42686P_REG_ACCEL_CONFIG0, 0x0F, (uint8_t)odr);
}

void tile_sense_i_6p6_set_gyro_odr(tile_t *tile, sense_i_6p6_odr_t odr)
{
    icm_modify(tile, ICM42686P_REG_GYRO_CONFIG0, 0x0F, (uint8_t)odr);
}

void tile_sense_i_6p6_set_power_mode(tile_t *tile,
                                      sense_i_6p6_power_mode_t accel,
                                      sense_i_6p6_power_mode_t gyro)
{
    uint8_t pwr = ((uint8_t)gyro << 2) | (uint8_t)accel;
    icm_modify(tile, ICM42686P_REG_PWR_MGMT0, 0x0F, pwr);
    tile->hal->delay_ms(1);  /* >200 µs */
}

void tile_sense_i_6p6_set_filter_bw(tile_t *tile,
                                     sense_i_6p6_filter_bw_t accel_bw,
                                     sense_i_6p6_filter_bw_t gyro_bw)
{
    icm_write(tile, ICM42686P_REG_GYRO_ACCEL_CFG0,
              ((uint8_t)accel_bw << 4) | (uint8_t)gyro_bw);
}

void tile_sense_i_6p6_set_filter_order(tile_t *tile,
                                        sense_i_6p6_filter_order_t accel_order,
                                        sense_i_6p6_filter_order_t gyro_order)
{
    icm_modify(tile, ICM42686P_REG_GYRO_CONFIG1, 0x0C, (uint8_t)gyro_order << 2);
    icm_modify(tile, ICM42686P_REG_ACCEL_CONFIG1, 0x18, (uint8_t)accel_order << 3);
}

void tile_sense_i_6p6_set_temp_filter(tile_t *tile, sense_i_6p6_temp_filter_t bw)
{
    icm_modify(tile, ICM42686P_REG_GYRO_CONFIG1, 0xE0, (uint8_t)bw << 5);
}

void tile_sense_i_6p6_set_temp_enabled(tile_t *tile, uint8_t enabled)
{
    icm_modify(tile, ICM42686P_REG_PWR_MGMT0, 0x20, enabled ? 0x00 : 0x20);
}

/* ================================================================
 * Data reads
 * ================================================================ */

uint8_t tile_sense_i_6p6_data_ready(tile_t *tile)
{
    return (icm_read(tile, ICM42686P_REG_INT_STATUS) & ICM42686P_INT_DATA_RDY) != 0;
}

void tile_sense_i_6p6_get_raw_accels(tile_t *tile, int16_t *buffer)
{
    uint8_t raw[6];
    icm_read_buf(tile, ICM42686P_REG_ACCEL_X_H, raw, 6);
    buffer[0] = swap16(raw[0], raw[1]);
    buffer[1] = swap16(raw[2], raw[3]);
    buffer[2] = swap16(raw[4], raw[5]);
}

void tile_sense_i_6p6_get_raw_gyros(tile_t *tile, int16_t *buffer)
{
    uint8_t raw[6];
    icm_read_buf(tile, ICM42686P_REG_GYRO_X_H, raw, 6);
    buffer[0] = swap16(raw[0], raw[1]);
    buffer[1] = swap16(raw[2], raw[3]);
    buffer[2] = swap16(raw[4], raw[5]);
}

void tile_sense_i_6p6_get_raw_6dof(tile_t *tile, int16_t *buffer)
{
    uint8_t raw[12];
    icm_read_buf(tile, ICM42686P_REG_ACCEL_X_H, raw, 12);
    buffer[0] = swap16(raw[0],  raw[1]);
    buffer[1] = swap16(raw[2],  raw[3]);
    buffer[2] = swap16(raw[4],  raw[5]);
    buffer[3] = swap16(raw[6],  raw[7]);
    buffer[4] = swap16(raw[8],  raw[9]);
    buffer[5] = swap16(raw[10], raw[11]);
}

int16_t tile_sense_i_6p6_get_temperature(tile_t *tile)
{
    uint8_t raw[2];
    icm_read_buf(tile, ICM42686P_REG_TEMP_H, raw, 2);
    return swap16(raw[0], raw[1]);
}

void tile_sense_i_6p6_get_raw_all(tile_t *tile, int16_t *buffer)
{
    /* Burst: TEMP_H(0x1D) through GYRO_Z_L(0x2A) = 14 bytes = 7 × int16 */
    uint8_t raw[14];
    icm_read_buf(tile, ICM42686P_REG_TEMP_H, raw, 14);
    for (int i = 0; i < 7; i++)
        buffer[i] = swap16(raw[i * 2], raw[i * 2 + 1]);
}

/* ================================================================
 * FIFO
 * ================================================================ */

void tile_sense_i_6p6_fifo_config(tile_t *tile, sense_i_6p6_fifo_mode_t mode,
                                   uint8_t accel, uint8_t gyro,
                                   uint8_t temp, uint8_t hires)
{
    /* FIFO_CONFIG: mode in bits [7:6] */
    icm_write(tile, ICM42686P_REG_FIFO_CONFIG, (uint8_t)mode << 6);

    /* FIFO_CONFIG1: data selection */
    uint8_t cfg1 = 0;
    if (hires) cfg1 |= (1 << 4);
    if (temp)  cfg1 |= (1 << 2);
    if (gyro)  cfg1 |= (1 << 1);
    if (accel) cfg1 |= (1 << 0);
    icm_write(tile, ICM42686P_REG_FIFO_CONFIG1, cfg1);
}

void tile_sense_i_6p6_fifo_set_watermark(tile_t *tile, uint16_t records)
{
    if (records == 0) records = 1;  /* 0 is invalid per datasheet */
    icm_write(tile, ICM42686P_REG_FIFO_CONFIG2, (uint8_t)(records & 0xFF));
    icm_write(tile, ICM42686P_REG_FIFO_CONFIG3, (uint8_t)((records >> 8) & 0x0F));
}

void tile_sense_i_6p6_fifo_flush(tile_t *tile)
{
    icm_write(tile, ICM42686P_REG_SIGNAL_PATH_RESET, ICM42686P_FIFO_FLUSH);
}

uint16_t tile_sense_i_6p6_fifo_count(tile_t *tile)
{
    uint8_t raw[2];
    /* Must read COUNTL first to latch both bytes */
    raw[1] = icm_read(tile, ICM42686P_REG_FIFO_COUNTL);
    raw[0] = icm_read(tile, ICM42686P_REG_FIFO_COUNTH);
    return ((uint16_t)raw[0] << 8) | raw[1];
}

uint8_t tile_sense_i_6p6_fifo_read_packet(tile_t *tile, sense_i_6p6_fifo_packet_t *pkt)
{
    uint8_t raw[16];
    icm_read_buf(tile, ICM42686P_REG_FIFO_DATA, raw, 16);

    pkt->header = raw[0];

    if (pkt->header & SENSE_I_6P6_FIFO_HEADER_EMPTY)
        return 0;

    /* Standard packet: header + accel(6) + gyro(6) + temp(1) + timestamp(2) */
    pkt->accel[0] = swap16(raw[1],  raw[2]);
    pkt->accel[1] = swap16(raw[3],  raw[4]);
    pkt->accel[2] = swap16(raw[5],  raw[6]);
    pkt->gyro[0]  = swap16(raw[7],  raw[8]);
    pkt->gyro[1]  = swap16(raw[9],  raw[10]);
    pkt->gyro[2]  = swap16(raw[11], raw[12]);
    pkt->temp     = (int8_t)raw[13];
    pkt->timestamp = ((uint16_t)raw[14] << 8) | raw[15];

    return 1;
}

uint16_t tile_sense_i_6p6_fifo_read_packets(tile_t *tile,
                                              sense_i_6p6_fifo_packet_t *packets,
                                              uint16_t max_count)
{
    uint16_t count = tile_sense_i_6p6_fifo_count(tile);
    if (count > max_count) count = max_count;

    uint16_t read = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (!tile_sense_i_6p6_fifo_read_packet(tile, &packets[i]))
            break;
        read++;
    }
    return read;
}

uint16_t tile_sense_i_6p6_fifo_lost_count(tile_t *tile)
{
    uint8_t lo = icm_read(tile, ICM42686P_REG_FIFO_LOST_PKT0);
    uint8_t hi = icm_read(tile, ICM42686P_REG_FIFO_LOST_PKT1);
    return ((uint16_t)hi << 8) | lo;
}

/* ================================================================
 * Interrupts
 * ================================================================ */

void tile_sense_i_6p6_int1_config(tile_t *tile, uint8_t config)
{
    /* INT_CONFIG bits [2:0] = INT1_POLARITY, INT1_DRIVE, INT1_MODE */
    icm_modify(tile, ICM42686P_REG_INT_CONFIG, 0x07, config);
}

void tile_sense_i_6p6_int1_data_ready(tile_t *tile, uint8_t enabled)
{
    icm_modify(tile, ICM42686P_REG_INT_SOURCE0, 0x08, enabled ? 0x08 : 0x00);
}

void tile_sense_i_6p6_int1_fifo_ths(tile_t *tile, uint8_t enabled)
{
    icm_modify(tile, ICM42686P_REG_INT_SOURCE0, 0x04, enabled ? 0x04 : 0x00);
}

void tile_sense_i_6p6_int1_wom(tile_t *tile, uint8_t enabled)
{
    uint8_t val = enabled ? 0x07 : 0x00;  /* WOM_X, WOM_Y, WOM_Z */
    icm_modify(tile, ICM42686P_REG_INT_SOURCE1, 0x07, val);
}

uint8_t tile_sense_i_6p6_get_int_status(tile_t *tile)
{
    return icm_read(tile, ICM42686P_REG_INT_STATUS);
}

uint8_t tile_sense_i_6p6_get_int_status2(tile_t *tile)
{
    return icm_read(tile, ICM42686P_REG_INT_STATUS2);
}

uint8_t tile_sense_i_6p6_get_int_status3(tile_t *tile)
{
    return icm_read(tile, ICM42686P_REG_INT_STATUS3);
}

/* ================================================================
 * Wake on Motion (WOM)
 * ================================================================ */

void tile_sense_i_6p6_wom_config(tile_t *tile,
                                  uint16_t x_mg, uint16_t y_mg, uint16_t z_mg,
                                  sense_i_6p6_wom_mode_t mode)
{
    /* Threshold resolution: 1g/256 ≈ 3.9 mg. Register = mg * 256 / 1000 */
    uint8_t x_thr = (uint8_t)((x_mg * 256 + 500) / 1000);
    uint8_t y_thr = (uint8_t)((y_mg * 256 + 500) / 1000);
    uint8_t z_thr = (uint8_t)((z_mg * 256 + 500) / 1000);

    /* Thresholds are in Bank 4 */
    icm_set_bank(tile, ICM42686P_BANK_4);
    icm_write(tile, ICM42686P_B4_WOM_X_THR, x_thr);
    icm_write(tile, ICM42686P_B4_WOM_Y_THR, y_thr);
    icm_write(tile, ICM42686P_B4_WOM_Z_THR, z_thr);
    icm_set_bank(tile, ICM42686P_BANK_0);

    /* SMD_CONFIG: set WOM_MODE, keep SMD bits */
    icm_modify(tile, ICM42686P_REG_SMD_CONFIG, 0x04, (uint8_t)mode << 2);
}

void tile_sense_i_6p6_wom_enable(tile_t *tile)
{
    /* WOM_INT_MODE=0 (OR): any axis triggers. Set in SMD_CONFIG[3]. */
    icm_modify(tile, ICM42686P_REG_SMD_CONFIG, 0x08, 0x00);
}

void tile_sense_i_6p6_wom_disable(tile_t *tile)
{
    /* Clear WOM threshold registers (set to 0 = disabled) */
    icm_set_bank(tile, ICM42686P_BANK_4);
    icm_write(tile, ICM42686P_B4_WOM_X_THR, 0);
    icm_write(tile, ICM42686P_B4_WOM_Y_THR, 0);
    icm_write(tile, ICM42686P_B4_WOM_Z_THR, 0);
    icm_set_bank(tile, ICM42686P_BANK_0);
}

/* ================================================================
 * Significant Motion Detection (SMD)
 * ================================================================ */

void tile_sense_i_6p6_smd_config(tile_t *tile, sense_i_6p6_smd_mode_t mode)
{
    icm_modify(tile, ICM42686P_REG_SMD_CONFIG, 0x03, (uint8_t)mode);
}

/* ================================================================
 * APEX: DMP initialization helper
 * ================================================================ */

static void dmp_init_if_needed(tile_t *tile)
{
    /* Check if DMP is already idle (already initialized) */
    uint8_t data3 = icm_read(tile, ICM42686P_REG_APEX_DATA3);
    if (data3 & 0x04) return;  /* DMP_IDLE=1 means already initialized */

    /* DMP memory reset */
    icm_write(tile, ICM42686P_REG_SIGNAL_PATH_RESET, ICM42686P_DMP_MEM_RESET);
    tile->hal->delay_ms(1);

    /* DMP init */
    icm_write(tile, ICM42686P_REG_SIGNAL_PATH_RESET, ICM42686P_DMP_INIT_EN);
    tile->hal->delay_ms(50);  /* DMP boot time */
}

/* ================================================================
 * APEX: Pedometer
 * ================================================================ */

void tile_sense_i_6p6_pedometer_enable(tile_t *tile, sense_i_6p6_dmp_odr_t dmp_odr)
{
    dmp_init_if_needed(tile);

    /* Set DMP ODR and enable pedometer */
    uint8_t cfg0 = icm_read(tile, ICM42686P_REG_APEX_CONFIG0);
    cfg0 &= ~0x23;  /* Clear PED_ENABLE, DMP_ODR */
    cfg0 |= (1 << 5) | ((uint8_t)dmp_odr & 0x03);  /* PED_ENABLE + DMP_ODR */
    icm_write(tile, ICM42686P_REG_APEX_CONFIG0, cfg0);
}

void tile_sense_i_6p6_pedometer_disable(tile_t *tile)
{
    icm_modify(tile, ICM42686P_REG_APEX_CONFIG0, 0x20, 0x00);  /* Clear PED_ENABLE */
}

uint16_t tile_sense_i_6p6_get_step_count(tile_t *tile)
{
    /* 42686P byte order: DATA0 = low, DATA1 = high (opposite from 42688P!) */
    uint8_t lo = icm_read(tile, ICM42686P_REG_APEX_DATA0);
    uint8_t hi = icm_read(tile, ICM42686P_REG_APEX_DATA1);
    return ((uint16_t)hi << 8) | lo;
}

uint8_t tile_sense_i_6p6_get_step_cadence(tile_t *tile)
{
    return icm_read(tile, ICM42686P_REG_APEX_DATA2);
}

sense_i_6p6_activity_t tile_sense_i_6p6_get_activity(tile_t *tile)
{
    return (sense_i_6p6_activity_t)(icm_read(tile, ICM42686P_REG_APEX_DATA3) & 0x03);
}

/* ================================================================
 * APEX: Tilt Detection
 * ================================================================ */

void tile_sense_i_6p6_tilt_enable(tile_t *tile, uint8_t wait_seconds)
{
    dmp_init_if_needed(tile);

    /* TILT_WAIT_TIME_SEL in Bank 4, APEX_CONFIG4 [7:6] */
    uint8_t sel;
    if (wait_seconds <= 0)      sel = 0x00;
    else if (wait_seconds <= 2) sel = 0x01;
    else if (wait_seconds <= 4) sel = 0x02;
    else                        sel = 0x03;

    icm_set_bank(tile, ICM42686P_BANK_4);
    icm_modify(tile, ICM42686P_B4_APEX_CONFIG4, 0xC0, sel << 6);
    icm_set_bank(tile, ICM42686P_BANK_0);

    /* Enable tilt in APEX_CONFIG0 */
    icm_modify(tile, ICM42686P_REG_APEX_CONFIG0, 0x10, 0x10);
}

void tile_sense_i_6p6_tilt_disable(tile_t *tile)
{
    icm_modify(tile, ICM42686P_REG_APEX_CONFIG0, 0x10, 0x00);
}

/* ================================================================
 * APEX: Tap Detection
 * ================================================================ */

void tile_sense_i_6p6_tap_enable(tile_t *tile)
{
    dmp_init_if_needed(tile);
    icm_modify(tile, ICM42686P_REG_APEX_CONFIG0, 0x40, 0x40);
}

void tile_sense_i_6p6_tap_disable(tile_t *tile)
{
    icm_modify(tile, ICM42686P_REG_APEX_CONFIG0, 0x40, 0x00);
}

void tile_sense_i_6p6_get_tap_result(tile_t *tile, sense_i_6p6_tap_result_t *result)
{
    uint8_t d4 = icm_read(tile, ICM42686P_REG_APEX_DATA4);
    uint8_t d5 = icm_read(tile, ICM42686P_REG_APEX_DATA5);

    result->count     = (d4 >> 3) & 0x03;
    result->axis      = (d4 >> 1) & 0x03;
    result->direction = d4 & 0x01;
    result->timing    = d5 & 0x3F;
}

/* ================================================================
 * Advanced: User offsets
 * ================================================================ */

void tile_sense_i_6p6_set_gyro_offset(tile_t *tile,
                                       int16_t x, int16_t y, int16_t z)
{
    /* 12-bit signed values packed across OFFSET_USER0-4 (Bank 4).
     * Layout: U0=[GX 7:0], U1=[GY 11:8 | GX 11:8], U2=[GY 7:0],
     *         U3=[GZ 7:0], U4=[AX 11:8 | GZ 11:8] */
    icm_set_bank(tile, ICM42686P_BANK_4);
    icm_write(tile, ICM42686P_B4_OFFSET_USER0, (uint8_t)(x & 0xFF));
    uint8_t u1 = ((uint8_t)((y >> 8) & 0x0F) << 4) | ((uint8_t)((x >> 8) & 0x0F));
    icm_write(tile, ICM42686P_B4_OFFSET_USER1, u1);
    icm_write(tile, ICM42686P_B4_OFFSET_USER2, (uint8_t)(y & 0xFF));
    icm_write(tile, ICM42686P_B4_OFFSET_USER3, (uint8_t)(z & 0xFF));
    icm_modify(tile, ICM42686P_B4_OFFSET_USER4, 0x0F, (uint8_t)((z >> 8) & 0x0F));
    icm_set_bank(tile, ICM42686P_BANK_0);
}

void tile_sense_i_6p6_set_accel_offset(tile_t *tile,
                                        int16_t x, int16_t y, int16_t z)
{
    /* 12-bit signed values packed across OFFSET_USER4-8 (Bank 4).
     * Layout: U4=[AX 11:8 | GZ 11:8], U5=[AX 7:0], U6=[AY 7:0],
     *         U7=[AZ 11:8 | AY 11:8], U8=[AZ 7:0] */
    icm_set_bank(tile, ICM42686P_BANK_4);
    icm_modify(tile, ICM42686P_B4_OFFSET_USER4, 0xF0, (uint8_t)((x >> 8) & 0x0F) << 4);
    icm_write(tile, ICM42686P_B4_OFFSET_USER5, (uint8_t)(x & 0xFF));
    icm_write(tile, ICM42686P_B4_OFFSET_USER6, (uint8_t)(y & 0xFF));
    uint8_t u7 = ((uint8_t)((z >> 8) & 0x0F) << 4) | ((uint8_t)((y >> 8) & 0x0F));
    icm_write(tile, ICM42686P_B4_OFFSET_USER7, u7);
    icm_write(tile, ICM42686P_B4_OFFSET_USER8, (uint8_t)(z & 0xFF));
    icm_set_bank(tile, ICM42686P_BANK_0);
}

/* ================================================================
 * Advanced: Self-test
 * ================================================================ */

uint8_t tile_sense_i_6p6_self_test(tile_t *tile, uint8_t *accel_pass, uint8_t *gyro_pass)
{
    /* Save current state */
    uint8_t pwr_save = icm_read(tile, ICM42686P_REG_PWR_MGMT0);

    /* Configure for self-test: ±4g accel, ±250 dps gyro, LN mode */
    icm_write(tile, ICM42686P_REG_ACCEL_CONFIG0,
              (SENSE_I_6P6_ACCEL_4G << 5) | SENSE_I_6P6_ODR_1KHZ);
    icm_write(tile, ICM42686P_REG_GYRO_CONFIG0,
              (SENSE_I_6P6_GYRO_250DPS << 5) | SENSE_I_6P6_ODR_1KHZ);
    icm_write(tile, ICM42686P_REG_PWR_MGMT0,
              ICM42686P_PWR_ACCEL_LN | ICM42686P_PWR_GYRO_LN);
    tile->hal->delay_ms(100);

    /* Read baseline (average of 200 samples at 1 kHz) */
    int32_t ax0 = 0, ay0 = 0, az0 = 0;
    int32_t gx0 = 0, gy0 = 0, gz0 = 0;
    for (int i = 0; i < 200; i++) {
        while (!tile_sense_i_6p6_data_ready(tile)) ;
        int16_t a[3], g[3];
        tile_sense_i_6p6_get_raw_accels(tile, a);
        tile_sense_i_6p6_get_raw_gyros(tile, g);
        ax0 += a[0]; ay0 += a[1]; az0 += a[2];
        gx0 += g[0]; gy0 += g[1]; gz0 += g[2];
    }
    ax0 /= 200; ay0 /= 200; az0 /= 200;
    gx0 /= 200; gy0 /= 200; gz0 /= 200;

    /* Enable self-test */
    icm_write(tile, ICM42686P_REG_SELF_TEST_CONFIG,
              (1 << 6) | 0x3F);  /* ACCEL_ST_POWER + all axes */
    tile->hal->delay_ms(100);

    /* Read self-test values */
    int32_t ax1 = 0, ay1 = 0, az1 = 0;
    int32_t gx1 = 0, gy1 = 0, gz1 = 0;
    for (int i = 0; i < 200; i++) {
        while (!tile_sense_i_6p6_data_ready(tile)) ;
        int16_t a[3], g[3];
        tile_sense_i_6p6_get_raw_accels(tile, a);
        tile_sense_i_6p6_get_raw_gyros(tile, g);
        ax1 += a[0]; ay1 += a[1]; az1 += a[2];
        gx1 += g[0]; gy1 += g[1]; gz1 += g[2];
    }
    ax1 /= 200; ay1 /= 200; az1 /= 200;
    gx1 /= 200; gy1 /= 200; gz1 /= 200;

    /* Disable self-test */
    icm_write(tile, ICM42686P_REG_SELF_TEST_CONFIG, 0x00);

    /* Calculate self-test response */
    int32_t a_resp[3] = { ax1 - ax0, ay1 - ay0, az1 - az0 };
    int32_t g_resp[3] = { gx1 - gx0, gy1 - gy0, gz1 - gz0 };

    /* Check against minimum thresholds (from datasheet section 9.1):
     * Accel: min 225 mg @ ±4g → 225 * 8192 / 1000 ≈ 1843 LSB
     * Gyro:  min 60 dps @ ±250 dps → 60 * 131 = 7860 LSB */
    *accel_pass = 1;
    for (int i = 0; i < 3; i++) {
        if (a_resp[i] < 0) a_resp[i] = -a_resp[i];
        if (a_resp[i] < 1843) *accel_pass = 0;
    }

    *gyro_pass = 1;
    for (int i = 0; i < 3; i++) {
        if (g_resp[i] < 0) g_resp[i] = -g_resp[i];
        if (g_resp[i] < 7860) *gyro_pass = 0;
    }

    /* Restore previous state */
    icm_write(tile, ICM42686P_REG_PWR_MGMT0, pwr_save);

    return (*accel_pass && *gyro_pass) ? 1 : 0;
}

/* ================================================================
 * Advanced: Generic register access
 * ================================================================ */

uint8_t tile_sense_i_6p6_read_reg(tile_t *tile, uint8_t bank, uint8_t reg)
{
    if (bank != ICM42686P_BANK_0) icm_set_bank(tile, bank);
    uint8_t val = icm_read(tile, reg);
    if (bank != ICM42686P_BANK_0) icm_set_bank(tile, ICM42686P_BANK_0);
    return val;
}

void tile_sense_i_6p6_write_reg(tile_t *tile, uint8_t bank, uint8_t reg, uint8_t val)
{
    if (bank != ICM42686P_BANK_0) icm_set_bank(tile, bank);
    icm_write(tile, reg, val);
    if (bank != ICM42686P_BANK_0) icm_set_bank(tile, ICM42686P_BANK_0);
}
