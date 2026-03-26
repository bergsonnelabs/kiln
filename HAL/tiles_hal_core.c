/**
 * @file   tiles_hal_core.c
 * @brief  Core tile HAL implementation.
 *
 * Currently wraps the STM32 HAL — identical callbacks to tiles_hal_stm32.c.
 * Kept as a separate file so that Core tiles can evolve independently
 * (DMA, multi-bus, non-STM32 variants) without affecting the generic
 * STM32 HAL.
 */

#include "tiles_hal_core.h"

#define CORE_HAL_TIMEOUT_MS  50

static const tiles_hal_core_cfg_t* s_cfg;

/* -------------------------------------------------------------- */
/* I2C callbacks                                                   */
/* -------------------------------------------------------------- */

static int core_i2c_read(void* handle, uint8_t addr, uint8_t reg,
                          uint8_t* data, uint16_t len)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    return (int)HAL_I2C_Mem_Read(hi2c, (uint16_t)(addr << 1), reg, 1,
                                  data, len, CORE_HAL_TIMEOUT_MS);
}

static int core_i2c_write(void* handle, uint8_t addr, uint8_t reg,
                           const uint8_t* data, uint16_t len)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    return (int)HAL_I2C_Mem_Write(hi2c, (uint16_t)(addr << 1), reg, 1,
                                   (uint8_t*)data, len, CORE_HAL_TIMEOUT_MS);
}

static int core_i2c_is_ready(void* handle, uint8_t addr)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    return (int)HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr << 1), 3,
                                       CORE_HAL_TIMEOUT_MS);
}

/* -------------------------------------------------------------- */
/* SPI callbacks                                                   */
/* -------------------------------------------------------------- */

static int core_spi_read(void* handle, uint8_t cs, uint8_t reg,
                          uint8_t* data, uint16_t len)
{
    (void)handle;
    SPI_HandleTypeDef* hspi = s_cfg->spi;

    HAL_GPIO_WritePin(s_cfg->spi_cs_ports[cs], s_cfg->spi_cs_pins[cs],
                      GPIO_PIN_RESET);

    uint8_t cmd = reg | 0x80;
    HAL_SPI_Transmit(hspi, &cmd, 1, CORE_HAL_TIMEOUT_MS);
    HAL_StatusTypeDef status = HAL_SPI_Receive(hspi, data, len,
                                                CORE_HAL_TIMEOUT_MS);

    HAL_GPIO_WritePin(s_cfg->spi_cs_ports[cs], s_cfg->spi_cs_pins[cs],
                      GPIO_PIN_SET);

    return (int)status;
}

static int core_spi_write(void* handle, uint8_t cs, uint8_t reg,
                           const uint8_t* data, uint16_t len)
{
    (void)handle;
    SPI_HandleTypeDef* hspi = s_cfg->spi;

    HAL_GPIO_WritePin(s_cfg->spi_cs_ports[cs], s_cfg->spi_cs_pins[cs],
                      GPIO_PIN_RESET);

    uint8_t cmd = reg & 0x7F;
    HAL_SPI_Transmit(hspi, &cmd, 1, CORE_HAL_TIMEOUT_MS);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(hspi, (uint8_t*)data, len,
                                                 CORE_HAL_TIMEOUT_MS);

    HAL_GPIO_WritePin(s_cfg->spi_cs_ports[cs], s_cfg->spi_cs_pins[cs],
                      GPIO_PIN_SET);

    return (int)status;
}

/* -------------------------------------------------------------- */
/* Delay                                                           */
/* -------------------------------------------------------------- */

static void core_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* -------------------------------------------------------------- */
/* Public init                                                      */
/* -------------------------------------------------------------- */

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
        hal->i2c_read     = NULL;
        hal->i2c_write    = NULL;
        hal->i2c_is_ready = NULL;
        hal->handle       = NULL;
    }

    /* SPI */
    if (cfg->buses & TILES_BUS_SPI) {
        hal->spi_read  = core_spi_read;
        hal->spi_write = core_spi_write;
    } else {
        hal->spi_read  = NULL;
        hal->spi_write = NULL;
    }

    /* Shared */
    hal->delay_ms = core_delay_ms;
    hal->on_error = NULL;
    hal->buses    = cfg->buses;
}
