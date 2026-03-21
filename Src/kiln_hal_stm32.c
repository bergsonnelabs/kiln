/**
 * @file   kiln_hal_stm32.c
 * @brief  STM32 HAL reference implementation of the Kiln platform layer.
 *
 * Bridges kiln_hal_t function pointers to the STM32 HAL I2C API.
 * Uses DMA transfers with synchronous completion wait — the CPU is
 * free to service interrupts (BLE, timers) during each I2C transfer.
 *
 * This is the only file in Kiln that includes STM32-specific headers.
 */

#include "kiln_hal.h"
#include "i2c.h"        /* STM32 HAL: I2C_HandleTypeDef, HAL_I2C_* */
#include "diag.h"       /* Runtime diagnostic counters */

#define KILN_I2C_TIMEOUT_MS  5   /* BLE-safe: normal I2C write <1ms at 100kHz */

/* -------------------------------------------------------------- */
/* Private: identify bus number (1 or 3) for diagnostic counters   */
/* -------------------------------------------------------------- */
static inline int is_bus1(I2C_HandleTypeDef* hi2c)
{
    return (hi2c->Instance == I2C1);
}

/* -------------------------------------------------------------- */
/* Private: wait for I2C peripheral to return to READY state       */
/* -------------------------------------------------------------- */
static int i2c_wait_ready(I2C_HandleTypeDef* hi2c)
{
    uint32_t start = HAL_GetTick();
    while (HAL_I2C_GetState(hi2c) != HAL_I2C_STATE_READY) {
        if ((HAL_GetTick() - start) > KILN_I2C_TIMEOUT_MS) {
            /* Abort any stuck DMA transfer */
            HAL_I2C_Master_Abort_IT(hi2c, (uint16_t)0);
            if (is_bus1(hi2c)) diag.i2c1_dma_error++;
            else               diag.i2c3_dma_error++;
            return (int)HAL_TIMEOUT;
        }
    }
    return (int)HAL_OK;
}

/* -------------------------------------------------------------- */
/* Private: callbacks matching kiln_hal_t function pointer sigs    */
/* -------------------------------------------------------------- */

static int stm32_i2c_read(void* handle, uint8_t addr, uint8_t reg,
                           uint8_t* data, uint16_t len)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read_DMA(hi2c, (uint16_t)(addr << 1), reg, 1,
                                   data, len);
    if (status != HAL_OK) {
        /* DMA start failed — fall back to blocking */
        if (is_bus1(hi2c)) diag.i2c1_dma_fallback++;
        else               diag.i2c3_dma_fallback++;
        int rc = (int)HAL_I2C_Mem_Read(hi2c, (uint16_t)(addr << 1), reg, 1,
                                        data, len, KILN_I2C_TIMEOUT_MS);
        if (rc != (int)HAL_OK) {
            if (is_bus1(hi2c)) diag.i2c1_error++;
            else               diag.i2c3_error++;
        }
        return rc;
    }

    return i2c_wait_ready(hi2c);
}

static int stm32_i2c_write(void* handle, uint8_t addr, uint8_t reg,
                            const uint8_t* data, uint16_t len)
{
    I2C_HandleTypeDef* hi2c = (I2C_HandleTypeDef*)handle;
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Write_DMA(hi2c, (uint16_t)(addr << 1), reg, 1,
                                    (uint8_t*)data, len);
    if (status != HAL_OK) {
        /* DMA start failed — fall back to blocking */
        if (is_bus1(hi2c)) diag.i2c1_dma_fallback++;
        else               diag.i2c3_dma_fallback++;
        int rc = (int)HAL_I2C_Mem_Write(hi2c, (uint16_t)(addr << 1), reg, 1,
                                         (uint8_t*)data, len, KILN_I2C_TIMEOUT_MS);
        if (rc != (int)HAL_OK) {
            if (is_bus1(hi2c)) diag.i2c1_error++;
            else               diag.i2c3_error++;
        }
        return rc;
    }

    return i2c_wait_ready(hi2c);
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
