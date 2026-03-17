/**
 * @file   kiln_hal_stm32.c
 * @brief  STM32 HAL reference implementation of the Kiln platform layer.
 *
 * Bridges kiln_hal_t function pointers to the STM32 HAL I2C API.
 * This is the only file in Kiln that includes STM32-specific headers.
 */

#include "kiln_hal.h"
#include "i2c.h"        /* STM32 HAL: I2C_HandleTypeDef, HAL_I2C_* */

#define KILN_I2C_TIMEOUT_MS  200

/* -------------------------------------------------------------- */
/* Private: callbacks matching kiln_hal_t function pointer sigs    */
/* -------------------------------------------------------------- */

static int stm32_i2c_read(void* handle, uint8_t addr, uint8_t reg,
                           uint8_t* data, uint16_t len)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    return (int)HAL_I2C_Mem_Read(hi2c, (uint16_t)(addr << 1), reg, 1,
                                 data, len, KILN_I2C_TIMEOUT_MS);
}

static int stm32_i2c_write(void* handle, uint8_t addr, uint8_t reg,
                            const uint8_t* data, uint16_t len)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    return (int)HAL_I2C_Mem_Write(hi2c, (uint16_t)(addr << 1), reg, 1,
                                  (uint8_t*)data, len, KILN_I2C_TIMEOUT_MS);
}

static int stm32_i2c_is_ready(void* handle, uint8_t addr)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    return (int)HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr << 1), 3,
                                       KILN_I2C_TIMEOUT_MS);
}

static void stm32_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* -------------------------------------------------------------- */
/* Public: one-call initializer                                    */
/* -------------------------------------------------------------- */

/**
 * @brief  Initialize a kiln_hal_t for use with an STM32 I2C peripheral.
 *
 * @param  hal    Pointer to the kiln_hal_t to populate
 * @param  hi2c   STM32 HAL I2C handle (e.g. &hi2c1)
 */
void kiln_hal_stm32_init(kiln_hal_t* hal, I2C_HandleTypeDef* hi2c)
{
    hal->i2c_read     = stm32_i2c_read;
    hal->i2c_write    = stm32_i2c_write;
    hal->i2c_is_ready = stm32_i2c_is_ready;
    hal->delay_ms     = stm32_delay_ms;
    hal->handle       = (void*)hi2c;
}
