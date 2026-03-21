/**
 * @file   tile_drive_h.c
 * @brief  LRA haptic driver for the Drive.H tile (DRV2605L).
 */

#include "tile_drive_h.h"

/* -------------------------------------------------------------- */
/* Module state                                                    */
/* -------------------------------------------------------------- */
static kiln_hal_t* hal_ptr = 0;
static uint8_t     dev_addr = 0;

/* -------------------------------------------------------------- */
/* Internal helpers                                                */
/* -------------------------------------------------------------- */

static void drv_write(uint8_t reg, uint8_t value) {
    hal_ptr->i2c_write(hal_ptr->handle, dev_addr, reg, &value, 1);
}

static uint8_t drv_read(uint8_t reg) {
    uint8_t val = 0;
    hal_ptr->i2c_read(hal_ptr->handle, dev_addr, reg, &val, 1);
    return val;
}

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

uint8_t tile_drive_h_find(kiln_hal_t* hal, uint8_t addr) {
    return (hal->i2c_is_ready(hal->handle, addr) == 0) ? 1 : 0;
}

uint8_t tile_drive_h_init(kiln_hal_t* hal, uint8_t addr) {
    hal_ptr  = hal;
    dev_addr = addr;

    /* Verify device is present */
    uint8_t status = drv_read(DRV2605L_REG_STATUS);
    if (status != DRV2605L_STATUS_DEFAULT) {
        return 0;
    }

    /* Exit standby */
    drv_write(DRV2605L_REG_MODE, 0x00);
    hal_ptr->delay_ms(400);

    /* LRA drive levels for VG0832013D (1.8Vrms rated, ~235Hz)
     * RATED_VOLTAGE = Vrms * 255 / 5.36 = 0x56 (1.8V)
     * OD_CLAMP      = Vpeak * 255 / 5.44 = 0x8C (3.0V overdrive)
     */
    drv_write(DRV2605L_REG_RATED_VOLTAGE, 0x56);
    drv_write(DRV2605L_REG_OD_CLAMP, 0x8C);

    /* Configure for LRA:
     * FEEDBACK_CTRL = 0xB6
     *   bit 7   = 1  N_ERM_LRA (LRA mode)
     *   bit 6-4 = 011  FB_BRAKE_FACTOR
     *   bit 3-2 = 01   LOOP_GAIN
     *   bit 1-0 = 10   BEMF_GAIN
     */
    drv_write(DRV2605L_REG_FEEDBACK_CTRL, 0xB6);
    hal_ptr->delay_ms(100);

    /* CONTROL3: LRA open loop */
    drv_write(DRV2605L_REG_CONTROL3, 0x01);

    /* Select LRA waveform library */
    drv_write(DRV2605L_REG_LIBRARY_SEL, 6);

    return 1;
}

void tile_drive_h_select(kiln_hal_t* hal, uint8_t addr) {
    hal_ptr  = hal;
    dev_addr = addr;
}

void tile_drive_h_play(uint8_t index, uint8_t repeats) {
    /* Load effect into sequence slot 0, terminate in slot 1 */
    drv_write(DRV2605L_REG_WAVE_SEQ_0, index);
    drv_write(DRV2605L_REG_WAVE_SEQ_1, 0);

    /* Trigger */
    drv_write(DRV2605L_REG_GO, 0x01);

    /* Repeat with gap */
    for (uint8_t i = 1; i < repeats; i++) {
        hal_ptr->delay_ms(200);
        drv_write(DRV2605L_REG_GO, 0x01);
    }
}

void tile_drive_h_stop(void) {
    drv_write(DRV2605L_REG_GO, 0x00);
}

void tile_drive_h_rtp_start(void) {
    /* Set MODE = 0x05 (RTP mode) */
    drv_write(DRV2605L_REG_MODE, 0x05);
    /* Write 0 to RTP register initially (silent) */
    drv_write(DRV2605L_REG_RTP, 0x00);
}

void tile_drive_h_rtp_write(uint8_t amplitude) {
    drv_write(DRV2605L_REG_RTP, amplitude);
}

void tile_drive_h_rtp_stop(void) {
    /* Silence the output */
    drv_write(DRV2605L_REG_RTP, 0x00);
    /* Return to internal trigger mode (library playback) */
    drv_write(DRV2605L_REG_MODE, 0x00);
}
