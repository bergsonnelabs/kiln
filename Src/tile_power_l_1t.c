/**
 * @file   tile_power_l_1t.c
 * @brief  Li-Ion charge controller implementation (BQ25150).
 */

#include "tile_power_l_1t.h"

/* -------------------------------------------------------------- */
/* Private state                                                   */
/* -------------------------------------------------------------- */

static kiln_hal_t* hal_ptr = 0;
static uint8_t bq_addr = BQ25150_I2C_ADDR_DEFAULT;

/* -------------------------------------------------------------- */
/* Private helpers                                                 */
/* -------------------------------------------------------------- */

static void bq_write(uint8_t reg, uint8_t value)
{
    hal_ptr->i2c_write(hal_ptr->handle, bq_addr, reg, &value, 1);
}

static uint8_t bq_read(uint8_t reg)
{
    uint8_t value = 0;
    hal_ptr->i2c_read(hal_ptr->handle, bq_addr, reg, &value, 1);
    return value;
}

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

uint8_t tile_power_l_1t_find(kiln_hal_t* hal, uint8_t addr)
{
    return (hal->i2c_is_ready(hal->handle, addr) == 0) ? 1 : 0;
}

uint8_t tile_power_l_1t_init(kiln_hal_t* hal, uint8_t addr)
{
    hal_ptr = hal;
    bq_addr = addr;

    /* Verify device ID */
    uint8_t id = bq_read(BQ25150_REG_DEVICE_ID);
    if (id != BQ25150_DEVICE_ID_DEFAULT) {
        return 0;
    }

    /* Fast charge current: 80 mA */
    bq_write(BQ25150_REG_ICHG_CTRL, 0x40);

    /* Precharge current: 20 mA */
    bq_write(BQ25150_REG_PCHRGCTRL, 0x0F);

    /* Battery undervoltage lockout: 2.6 V */
    bq_write(BQ25150_REG_BUVLO, 0x04);

    /* Enable VBAT ADC channel */
    bq_write(BQ25150_REG_ADC_READ_EN, 0x08);

    /* ADC: 1-second update rate in battery mode */
    bq_write(BQ25150_REG_ADCCTRL0, 0x82);

    /* Disable ship mode */
    bq_write(BQ25150_REG_ICCTRL0, 0x00);

    return 1;
}

void tile_power_l_1t_select(kiln_hal_t* hal, uint8_t addr)
{
    hal_ptr = hal;
    bq_addr = addr;
}

uint16_t tile_power_l_1t_get_vbat(void)
{
    uint16_t result;
    uint8_t msb = bq_read(BQ25150_REG_VBAT_MSB);
    uint8_t lsb = bq_read(BQ25150_REG_VBAT_LSB);
    result = ((uint16_t)msb << 8) | lsb;
    return result;
}

uint8_t tile_power_l_1t_get_percent(void)
{
    uint8_t result = 0;
    uint16_t voltage = tile_power_l_1t_get_vbat();
    if (voltage < 38229) {
        /* Below minimum threshold — report 0% */
        result = 0;
    } else {
        result = (uint8_t)((voltage / 76.458) - 500);
    }
    if (result > 100) {
        result = 100;
    }
    return result;
}

uint8_t tile_power_l_1t_read_status(uint8_t reg)
{
    return bq_read(reg);
}

void tile_power_l_1t_write_reg(uint8_t reg, uint8_t value)
{
    bq_write(reg, value);
}
