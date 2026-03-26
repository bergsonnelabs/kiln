/**
 * @file   tiles_hal_core.c
 * @brief  Core tile HAL — native Cores SDK implementation.
 *
 * Implements tiles_hal_t callbacks using the Cores firmware SDK's
 * LL/HAL layer directly.  No CubeIDE HAL dependency.
 *
 * I2C:  Wraps hal_i2c_read_reg / hal_i2c_write_reg / hal_i2c_probe.
 * SPI:  Wraps hal_spi_select / hal_spi_write / hal_spi_read / hal_spi_deselect
 *       with per-CS pin management via ll_gpio.
 * Delay: Uses ll_delay_ms (SysTick-based).
 *
 * The Cores SDK I2C functions take 7-bit addresses directly (no shifting)
 * and handle START/STOP/repeated-START internally.
 */

#include "tiles_hal_core.h"

/* Cores SDK headers */
#include "hal_i2c.h"
#include "hal_spi.h"
#include "ll_gpio.h"
#include "ll_systick.h"

/* ------------------------------------------------------------------ */
/* Static config reference                                             */
/* ------------------------------------------------------------------ */

static const tiles_hal_core_cfg_t* s_cfg;

/* ------------------------------------------------------------------ */
/* I2C callbacks                                                       */
/* ------------------------------------------------------------------ */

/**
 * Register read via Cores SDK I2C.
 * Cores hal_i2c_read_reg takes 7-bit addr directly — no shift needed.
 */
static int core_i2c_read(void* handle, uint8_t addr, uint8_t reg,
                         uint8_t* data, uint16_t len)
{
    hal_i2c_t* h = (hal_i2c_t*)handle;
    return (hal_i2c_read_reg(h, addr, reg, data, (uint32_t)len) == HAL_OK)
           ? 0 : -1;
}

/**
 * Register write via Cores SDK I2C.
 */
static int core_i2c_write(void* handle, uint8_t addr, uint8_t reg,
                          const uint8_t* data, uint16_t len)
{
    hal_i2c_t* h = (hal_i2c_t*)handle;
    return (hal_i2c_write_reg(h, addr, reg, data, (uint32_t)len) == HAL_OK)
           ? 0 : -1;
}

/**
 * Device presence check via Cores SDK I2C probe.
 */
static int core_i2c_is_ready(void* handle, uint8_t addr)
{
    hal_i2c_t* h = (hal_i2c_t*)handle;
    return (hal_i2c_probe(h, addr) == HAL_OK) ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* SPI callbacks                                                       */
/* ------------------------------------------------------------------ */

/**
 * SPI register read with per-CS pin management.
 *
 * Protocol: pull CS low → send (reg | 0x80) → read N bytes → CS high.
 * The 0x80 bit signals a read to most SPI peripherals.
 */
static int core_spi_read(void* handle, uint8_t cs, uint8_t reg,
                         uint8_t* data, uint16_t len)
{
    (void)handle;
    hal_spi_t* h = s_cfg->spi;

    /* Assert CS */
    ll_gpio_clear((GPIO_TypeDef*)s_cfg->cs[cs].port,
                  1UL << s_cfg->cs[cs].pin);

    /* Send read command (register | read bit) */
    uint8_t cmd = reg | 0x80;
    hal_spi_write(h, &cmd, 1);

    /* Clock in data */
    hal_spi_read(h, data, (uint32_t)len);

    /* Deassert CS */
    ll_gpio_set((GPIO_TypeDef*)s_cfg->cs[cs].port,
                1UL << s_cfg->cs[cs].pin);

    return 0;
}

/**
 * SPI register write with per-CS pin management.
 *
 * Protocol: pull CS low → send (reg & 0x7F) → write N bytes → CS high.
 */
static int core_spi_write(void* handle, uint8_t cs, uint8_t reg,
                          const uint8_t* data, uint16_t len)
{
    (void)handle;
    hal_spi_t* h = s_cfg->spi;

    /* Assert CS */
    ll_gpio_clear((GPIO_TypeDef*)s_cfg->cs[cs].port,
                  1UL << s_cfg->cs[cs].pin);

    /* Send write command (register with read bit cleared) */
    uint8_t cmd = reg & 0x7F;
    hal_spi_write(h, &cmd, 1);

    /* Send data */
    hal_spi_write(h, data, (uint32_t)len);

    /* Deassert CS */
    ll_gpio_set((GPIO_TypeDef*)s_cfg->cs[cs].port,
                1UL << s_cfg->cs[cs].pin);

    return 0;
}

/* ------------------------------------------------------------------ */
/* Delay                                                               */
/* ------------------------------------------------------------------ */

/**
 * Millisecond delay via Cores SDK SysTick.
 */
static void core_delay_ms(uint32_t ms)
{
    ll_delay_ms(ms);
}

/* ------------------------------------------------------------------ */
/* Public init                                                         */
/* ------------------------------------------------------------------ */

void tiles_hal_core_init(tiles_hal_t* hal, const tiles_hal_core_cfg_t* cfg)
{
    s_cfg = cfg;

    /* I2C */
    if (cfg->buses & TILES_BUS_I2C) {
        hal->i2c_read     = core_i2c_read;
        hal->i2c_write    = core_i2c_write;
        hal->i2c_is_ready = core_i2c_is_ready;
        hal->handle       = (void*)cfg->i2c;
    } else {
        hal->i2c_read     = (void*)0;
        hal->i2c_write    = (void*)0;
        hal->i2c_is_ready = (void*)0;
        hal->handle       = (void*)0;
    }

    /* SPI */
    if (cfg->buses & TILES_BUS_SPI) {
        hal->spi_read  = core_spi_read;
        hal->spi_write = core_spi_write;
    } else {
        hal->spi_read  = (void*)0;
        hal->spi_write = (void*)0;
    }

    /* Shared */
    hal->delay_ms = core_delay_ms;
    hal->on_error = (void*)0;
    hal->buses    = cfg->buses;
}
