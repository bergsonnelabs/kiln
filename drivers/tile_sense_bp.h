/**
 * @file  tile_sense_bp.h
 * @brief ILPS22QS barometric pressure & temperature sensor.
 *
 * Platform-agnostic driver for the ST ILPS22QS dual full-scale absolute
 * pressure sensor (260–1260 hPa or 260–4060 hPa) with embedded temperature
 * sensor, 128-sample FIFO, low-pass filter, and Qvar electrostatic sensing.
 *
 * Key specifications:
 *  - 24-bit pressure output, 0.5 hPa absolute accuracy (mode 1)
 *  - 16-bit temperature output, 100 LSB/°C
 *  - Output data rates from 1 Hz to 200 Hz
 *  - Configurable averaging (4 to 512 samples)
 *  - 128-slot pressure FIFO with watermark interrupt
 *  - One-shot mode for ultra-low-power operation
 *  - Pressure threshold interrupts with autozero/autorefp
 *  - One-point calibration via hardware offset registers
 *
 * Quick start:
 * @code
 *   #include "core.h"
 *   #include "core_tiles.h"
 *   #include "tile_sense_bp.h"
 *
 *   tile_t baro;
 *   sense_bp_cfg_t cfg = { .odr = SENSE_BP_ODR_25HZ };
 *   tile_sense_bp_init(core_tiles_pal(&core_i2c3), 0, &baro, &cfg);
 *
 *   int32_t pressure_mhpa = tile_sense_bp_get_pressure_mhpa(&baro);
 *   int32_t temp_cdeg     = tile_sense_bp_get_temp_cdeg(&baro);
 * @endcode
 *
 * Datasheet: https://www.st.com/resource/en/datasheet/ilps22qs.pdf
 *
 * @tessera tile label=Sense.BP icon=◇
 *
 * Driver gaps (chip capabilities not exposed by this driver):
 *
 * @tessera unsupported severity=common category="Qvar charge-variation sensing"
 *   ILPS22QS has a charge-variation sensing mode that uses pressure-port
 *   capacitance changes for proximity / touch detection. Driver explicitly
 *   disables Qvar at init and doesn't expose it — opening up Qvar would
 *   unlock proximity-style use cases on a pressure tile.
 *
 * @tessera unsupported severity=niche category="Analog Hub (AH) mode"
 *   ILPS22QS can repurpose its I²C lines as analog inputs (AH_QVAR_EN).
 *   Driver doesn't expose this mode; advanced sensor-fusion projects
 *   might want it.
 *
 * @tessera unsupported severity=advanced category="Alternate bus modes (SPI / I3C)"
 *   Driver is I²C-only. ILPS22QS also supports 3-wire and 4-wire SPI
 *   (selectable via IF_CTRL/SIM) and I3C (in-band interrupts, hot-join,
 *   dynamic addressing). SPI would unlock higher sample rates without
 *   bus contention; I3C lowers power and latency once Tessera adds an
 *   I3C bus.
 *
 * @tessera unsupported severity=niche category="Boot status"
 *   Chip exposes a BOOT bit in INT_SOURCE / IF_CTRL that signals
 *   power-on completion. Driver doesn't expose it; matters when
 *   interrogating the chip immediately after VDD comes up.
 *
 * @note All bus I/O is routed through tiles_pal_t function pointers.
 *       This driver contains no platform-specific code.
 */

#ifndef TILE_SENSE_BP_H
#define TILE_SENSE_BP_H

#include "tiles.h"

/* ---- Driver version ---- */

#define TILE_SENSE_BP_VERSION_MAJOR  1
#define TILE_SENSE_BP_VERSION_MINOR  0
#define TILE_SENSE_BP_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);

/* ---- Instance mapping ----
 *
 *  Instance  |  I2C address  |  AD0 pin state
 * -----------|---------------|------------------
 *     0      |     0x5D      |  Float / high (default)
 *     1      |     0x5C      |  Connected to GND
 */

/* ---- I2C addresses ---- */

#define ILPS22QS_I2C_ADDR_DEFAULT   0x5D  /**< AD0 float/high (default) */
#define ILPS22QS_I2C_ADDR_ALT       0x5C  /**< AD0 to GND */

/* ---- Register map ---- */

#define ILPS22QS_REG_INTERRUPT_CFG  0x0B
#define ILPS22QS_REG_THS_P_L       0x0C
#define ILPS22QS_REG_THS_P_H       0x0D
#define ILPS22QS_REG_IF_CTRL       0x0E
#define ILPS22QS_REG_WHO_AM_I      0x0F
#define ILPS22QS_REG_CTRL_REG1     0x10
#define ILPS22QS_REG_CTRL_REG2     0x11
#define ILPS22QS_REG_CTRL_REG3     0x12
#define ILPS22QS_REG_FIFO_CTRL     0x14
#define ILPS22QS_REG_FIFO_WTM      0x15
#define ILPS22QS_REG_REF_P_L       0x16
#define ILPS22QS_REG_REF_P_H       0x17
#define ILPS22QS_REG_I3C_IF_CTRL   0x19
#define ILPS22QS_REG_RPDS_L        0x1A
#define ILPS22QS_REG_RPDS_H        0x1B
#define ILPS22QS_REG_INT_SOURCE    0x24
#define ILPS22QS_REG_FIFO_STATUS1  0x25
#define ILPS22QS_REG_FIFO_STATUS2  0x26
#define ILPS22QS_REG_STATUS        0x27
#define ILPS22QS_REG_PRESS_OUT_XL  0x28
#define ILPS22QS_REG_PRESS_OUT_L   0x29
#define ILPS22QS_REG_PRESS_OUT_H   0x2A
#define ILPS22QS_REG_TEMP_OUT_L    0x2B
#define ILPS22QS_REG_TEMP_OUT_H    0x2C
#define ILPS22QS_REG_FIFO_DATA_OUT_PRESS_XL  0x78
#define ILPS22QS_REG_FIFO_DATA_OUT_PRESS_L   0x79
#define ILPS22QS_REG_FIFO_DATA_OUT_PRESS_H   0x7A

/* ---- Device ID ---- */

#define ILPS22QS_WHO_AM_I_VALUE    0xB4

/* ---- CTRL_REG2 bit masks ---- */

#define ILPS22QS_CTRL2_BOOT        (1 << 7)
#define ILPS22QS_CTRL2_FS_MODE     (1 << 6)
#define ILPS22QS_CTRL2_LFPF_CFG    (1 << 5)
#define ILPS22QS_CTRL2_EN_LPFP     (1 << 4)
#define ILPS22QS_CTRL2_BDU         (1 << 3)
#define ILPS22QS_CTRL2_SWRESET     (1 << 1)
#define ILPS22QS_CTRL2_ONESHOT     (1 << 0)

/* ---- STATUS bit masks ---- */

#define ILPS22QS_STATUS_T_OR       (1 << 5)
#define ILPS22QS_STATUS_P_OR       (1 << 4)
#define ILPS22QS_STATUS_T_DA       (1 << 1)
#define ILPS22QS_STATUS_P_DA       (1 << 0)

/* ---- FIFO_STATUS2 bit masks ---- */

#define ILPS22QS_FIFO_WTM_IA      (1 << 7)
#define ILPS22QS_FIFO_OVR_IA      (1 << 6)
#define ILPS22QS_FIFO_FULL_IA     (1 << 5)

/* ---- INT_SOURCE bit masks ---- */

#define ILPS22QS_INT_SRC_BOOT_ON  (1 << 7)
#define ILPS22QS_INT_SRC_IA       (1 << 2)
#define ILPS22QS_INT_SRC_PL       (1 << 1)
#define ILPS22QS_INT_SRC_PH       (1 << 0)

/* ---- INTERRUPT_CFG bit masks ---- */

#define ILPS22QS_INTCFG_AUTOREFP  (1 << 7)
#define ILPS22QS_INTCFG_RESET_ARP (1 << 6)
#define ILPS22QS_INTCFG_AUTOZERO  (1 << 5)
#define ILPS22QS_INTCFG_RESET_AZ  (1 << 4)
#define ILPS22QS_INTCFG_LIR       (1 << 2)
#define ILPS22QS_INTCFG_PLE       (1 << 1)
#define ILPS22QS_INTCFG_PHE       (1 << 0)

/* ---- Enumerations ---- */

/** Output data rate selection (CTRL_REG1 ODR[3:0], bits 6:3). */
typedef enum {
    SENSE_BP_ODR_POWERDOWN = 0x00,  /**< Power-down / one-shot */
    SENSE_BP_ODR_1HZ       = 0x01,  /**< 1 Hz */
    SENSE_BP_ODR_4HZ       = 0x02,  /**< 4 Hz */
    SENSE_BP_ODR_10HZ      = 0x03,  /**< 10 Hz */
    SENSE_BP_ODR_25HZ      = 0x04,  /**< 25 Hz */
    SENSE_BP_ODR_50HZ      = 0x05,  /**< 50 Hz */
    SENSE_BP_ODR_75HZ      = 0x06,  /**< 75 Hz */
    SENSE_BP_ODR_100HZ     = 0x07,  /**< 100 Hz */
    SENSE_BP_ODR_200HZ     = 0x08,  /**< 200 Hz */
} sense_bp_odr_t;

/** Averaging selection (CTRL_REG1 AVG[2:0], bits 2:0). */
typedef enum {
    SENSE_BP_AVG_4   = 0x00,  /**< 4 samples */
    SENSE_BP_AVG_8   = 0x01,  /**< 8 samples */
    SENSE_BP_AVG_16  = 0x02,  /**< 16 samples */
    SENSE_BP_AVG_32  = 0x03,  /**< 32 samples */
    SENSE_BP_AVG_64  = 0x04,  /**< 64 samples */
    SENSE_BP_AVG_128 = 0x05,  /**< 128 samples */
    SENSE_BP_AVG_512 = 0x07,  /**< 512 samples */
} sense_bp_avg_t;

/** Full-scale mode selection (CTRL_REG2 FS_MODE, bit 6). */
typedef enum {
    SENSE_BP_FS_1260HPA = 0,  /**< Mode 1: 260–1260 hPa, 4096 LSB/hPa */
    SENSE_BP_FS_4060HPA = 1,  /**< Mode 2: 260–4060 hPa, 2048 LSB/hPa */
} sense_bp_fs_t;

/** Low-pass filter bandwidth (CTRL_REG2 LFPF_CFG, bit 5). */
typedef enum {
    SENSE_BP_LPF_ODR_4 = 0,  /**< Bandwidth = ODR/4 */
    SENSE_BP_LPF_ODR_9 = 1,  /**< Bandwidth = ODR/9 */
} sense_bp_lpf_bw_t;

/** FIFO mode selection (FIFO_CTRL F_MODE[1:0] + TRIG_MODES). */
typedef enum {
    SENSE_BP_FIFO_BYPASS     = 0x00,  /**< FIFO disabled */
    SENSE_BP_FIFO_FIFO       = 0x01,  /**< Stop when full */
    SENSE_BP_FIFO_CONTINUOUS = 0x02,  /**< Dynamic stream, overwrites oldest */
    SENSE_BP_FIFO_BYP2FIFO   = 0x05,  /**< Bypass-to-FIFO on trigger */
    SENSE_BP_FIFO_BYP2CONT   = 0x06,  /**< Bypass-to-continuous on trigger */
    SENSE_BP_FIFO_CONT2FIFO  = 0x07,  /**< Continuous-to-FIFO on trigger */
} sense_bp_fifo_mode_t;

/* ---- Configuration struct ---- */

/**
 * @brief Optional init-time configuration for Sense.BP.
 *
 * Pass NULL to tile_sense_bp_init() for defaults:
 * ODR=25 Hz, AVG=4, FS=1260 hPa, LPF enabled at ODR/4, BDU enabled.
 */
typedef struct {
    uint8_t odr;       /**< Output data rate (sense_bp_odr_t). Default: SENSE_BP_ODR_25HZ. */
    uint8_t avg;       /**< Averaging depth (sense_bp_avg_t). Default: SENSE_BP_AVG_4. */
    uint8_t fs;        /**< Full-scale mode (sense_bp_fs_t). Default: SENSE_BP_FS_1260HPA. */
    uint8_t lpf;       /**< Low-pass filter: 1 = enabled (default), 0 = disabled. */
    uint8_t lpf_bw;    /**< LPF bandwidth (sense_bp_lpf_bw_t). Default: SENSE_BP_LPF_ODR_4. */
    uint8_t bdu;       /**< Block data update: 1 = enabled (default), 0 = continuous. */
} sense_bp_cfg_t;

/* ---- Lifecycle ---- */

/**
 * @brief  Check if a Sense.BP is present on the bus.
 * @param  hal      Platform HAL handle.
 * @param  instance 0 = default address (0x5D), 1 = alternate (0x5C).
 * @return 1 if device ACKs and WHO_AM_I matches, 0 otherwise.
 */
uint8_t tile_sense_bp_find(tiles_pal_t *hal, uint8_t instance);

/**
 * @brief  Initialise a Sense.BP tile.
 *
 * Probes the device, verifies WHO_AM_I, configures ODR/AVG/FS/LPF/BDU,
 * disables AH/Qvar for lower power, and sets tile state to READY.
 *
 * @param  hal      Platform HAL handle.
 * @param  instance 0 = default (0x5D), 1 = alternate (0x5C).
 * @param  tile     Tile handle to initialise.
 * @param  cfg      Configuration (NULL for defaults).
 */
void tile_sense_bp_init(tiles_pal_t *hal, uint8_t instance,
                        tile_t *tile, const sense_bp_cfg_t *cfg);

/**
 * @brief  Enter power-down mode (ODR = 0).
 * @tessera expose category=tile name=sleep
 * @param  tile  Initialised tile handle.
 */
void tile_sense_bp_sleep(tile_t *tile);

/**
 * @brief  Resume from power-down using cached ODR/AVG settings.
 * @tessera expose category=tile name=wake
 * @param  tile  Sleeping tile handle.
 */
void tile_sense_bp_wake(tile_t *tile);

/**
 * @brief  Software-reset the device.
 *
 * All registers return to defaults. Call init() again after reset.
 *
 * @tessera expose category=tile name=reset
 * @param  tile  Tile handle.
 */
void tile_sense_bp_reset(tile_t *tile);

/* ---- Configuration ---- */

/**
 * @brief  Set the output data rate.
 * @tessera expose category=tile name=set_odr
 * @param  tile  Initialised tile handle.
 * @param  odr   Desired output data rate.
 */
void tile_sense_bp_set_odr(tile_t *tile, sense_bp_odr_t odr);

/**
 * @brief  Set the averaging filter depth.
 * @tessera expose category=tile name=set_avg
 * @param  tile  Initialised tile handle.
 * @param  avg   Desired averaging.
 */
void tile_sense_bp_set_avg(tile_t *tile, sense_bp_avg_t avg);

/**
 * @brief  Set the full-scale mode.
 * @tessera expose category=tile name=set_fullscale
 * @param  tile  Initialised tile handle.
 * @param  fs    Full-scale selection.
 */
void tile_sense_bp_set_fullscale(tile_t *tile, sense_bp_fs_t fs);

/**
 * @brief  Enable or disable the low-pass filter.
 * @tessera expose category=tile name=set_lpf
 * @param  tile    Initialised tile handle.
 * @param  enable  1 = enable, 0 = disable.
 * @param  bw      Bandwidth selection (only used if enable = 1).
 */
void tile_sense_bp_set_lpf(tile_t *tile, uint8_t enable, sense_bp_lpf_bw_t bw);

/* ---- Pressure data ---- */

/**
 * @brief  Read the raw 24-bit pressure output (two's complement).
 * @tessera expose category=tile name=get_pressure_raw returns=int
 * @param  tile  Initialised tile handle.
 * @return Raw 24-bit signed value, sign-extended to int32_t.
 */
int32_t tile_sense_bp_get_pressure_raw(tile_t *tile);

/**
 * @brief  Read pressure in milli-hectopascals (integer, no float).
 * @tessera expose category=tile name=get_pressure_mhpa returns=int
 *
 * Returns pressure * 1000 in mhPa units. For example, 1013250 = 1013.250 hPa.
 * Accounts for the current full-scale mode setting.
 *
 * @param  tile  Initialised tile handle.
 * @return Pressure in milli-hPa (mhPa).
 */
int32_t tile_sense_bp_get_pressure_mhpa(tile_t *tile);

/* ---- Temperature data ---- */

/**
 * @brief  Read the raw 16-bit temperature output (two's complement).
 * @tessera expose category=tile name=get_temp_raw returns=int
 * @param  tile  Initialised tile handle.
 * @return Raw 16-bit signed value, sign-extended to int16_t.
 */
int16_t tile_sense_bp_get_temp_raw(tile_t *tile);

/**
 * @brief  Read temperature in centi-degrees Celsius (integer, no float).
 * @tessera expose category=tile name=get_temp_cdeg returns=int
 *
 * Returns temperature * 100. For example, 2534 = 25.34 °C.
 * Sensor sensitivity is 100 LSB/°C, so this is (raw * 100) / 100 = raw.
 *
 * @param  tile  Initialised tile handle.
 * @return Temperature in centi-°C.
 */
int32_t tile_sense_bp_get_temp_cdeg(tile_t *tile);

/* ---- One-shot mode ---- */

/**
 * @brief  Trigger a single measurement in power-down mode.
 * @tessera expose category=tile name=oneshot
 *
 * ODR must be POWERDOWN. Sets the ONESHOT bit in CTRL_REG2.
 * The bit self-clears when the measurement is complete.
 *
 * @param  tile  Tile handle in power-down mode.
 */
void tile_sense_bp_oneshot(tile_t *tile);

/* ---- Status ---- */

/**
 * @brief  Read the STATUS register.
 * @tessera expose category=tile name=get_status returns=int
 * @param  tile  Initialised tile handle.
 * @return Raw STATUS byte (use ILPS22QS_STATUS_* masks).
 */
uint8_t tile_sense_bp_get_status(tile_t *tile);

/**
 * @brief  Check if new pressure data is available.
 * @tessera expose category=tile name=pressure_ready returns=bool
 * @param  tile  Initialised tile handle.
 * @return 1 if P_DA is set, 0 otherwise.
 */
uint8_t tile_sense_bp_pressure_ready(tile_t *tile);

/**
 * @brief  Check if new temperature data is available.
 * @tessera expose category=tile name=temp_ready returns=bool
 * @param  tile  Initialised tile handle.
 * @return 1 if T_DA is set, 0 otherwise.
 */
uint8_t tile_sense_bp_temp_ready(tile_t *tile);

/* ---- FIFO ---- */

/**
 * @brief  Configure the FIFO mode.
 * @tessera expose category=tile name=set_fifo_mode
 * @param  tile  Initialised tile handle.
 * @param  mode  FIFO mode selection.
 */
void tile_sense_bp_set_fifo_mode(tile_t *tile, sense_bp_fifo_mode_t mode);

/**
 * @brief  Set the FIFO watermark threshold (0–127).
 * @tessera expose category=tile name=set_fifo_watermark
 * @param  tile       Initialised tile handle.
 * @param  watermark  Threshold level (0–127).
 */
void tile_sense_bp_set_fifo_watermark(tile_t *tile, uint8_t watermark);

/**
 * @brief  Read the number of unread FIFO samples.
 * @tessera expose category=tile name=get_fifo_level returns=int
 * @param  tile  Initialised tile handle.
 * @return Number of unread samples (0–128).
 */
uint8_t tile_sense_bp_get_fifo_level(tile_t *tile);

/**
 * @brief  Read FIFO status flags.
 * @tessera expose category=tile name=get_fifo_status returns=int
 * @param  tile  Initialised tile handle.
 * @return Raw FIFO_STATUS2 byte (use ILPS22QS_FIFO_* masks).
 */
uint8_t tile_sense_bp_get_fifo_status(tile_t *tile);

/**
 * @brief  Read one raw 24-bit pressure sample from the FIFO.
 * @tessera expose category=tile name=read_fifo_raw returns=int
 * @param  tile  Initialised tile handle.
 * @return Raw 24-bit signed pressure value, sign-extended to int32_t.
 */
int32_t tile_sense_bp_read_fifo_raw(tile_t *tile);

/**
 * @brief  Read multiple raw pressure samples from the FIFO.
 *
 * @tessera expose category=tile name=read_fifo_batch returns=int
 * @tessera out_buffer buf type=int32_t cap_param=count
 * @param  tile   Initialised tile handle.
 * @param  buf    Caller-allocated buffer the driver fills with raw
 *                24-bit signed values (sign-extended to int32_t).
 * @param  count  Capacity of `buf` — the driver fills up to this
 *                many samples and returns the actual count.
 * @return Number of samples actually read.
 */
uint8_t tile_sense_bp_read_fifo_batch(tile_t *tile, int32_t *buf,
                                      uint8_t count);

/* ---- Interrupt / threshold ---- */

/**
 * @brief  Set the pressure interrupt threshold in hPa.
 * @tessera expose category=tile name=set_threshold_hpa
 *
 * The threshold is applied to the differential pressure (P_DIFF_IN).
 * Enable PHE/PLE bits in INTERRUPT_CFG to generate interrupts.
 *
 * @param  tile    Initialised tile handle.
 * @param  ths_hpa Threshold in hPa (unsigned, applied symmetrically).
 */
void tile_sense_bp_set_threshold_hpa(tile_t *tile, uint16_t ths_hpa);

/**
 * @brief  Configure the interrupt source register.
 * @tessera expose category=tile name=set_interrupt_cfg
 * @param  tile  Initialised tile handle.
 * @param  cfg   Raw INTERRUPT_CFG byte (use ILPS22QS_INTCFG_* masks).
 */
void tile_sense_bp_set_interrupt_cfg(tile_t *tile, uint8_t cfg);

/**
 * @brief  Read the interrupt source register (clears latched flags).
 * @tessera expose category=tile name=get_int_source returns=int
 * @param  tile  Initialised tile handle.
 * @return Raw INT_SOURCE byte (use ILPS22QS_INT_SRC_* masks).
 */
uint8_t tile_sense_bp_get_int_source(tile_t *tile);

/* ---- Reference / offset calibration ---- */

/**
 * @brief  Enable autozero mode.
 * @tessera expose category=tile name=set_autozero
 *
 * Captures current pressure as REF_P. Output registers then show
 * the difference from reference. Reset with reset_autozero().
 *
 * @param  tile  Initialised tile handle.
 */
void tile_sense_bp_set_autozero(tile_t *tile);

/**
 * @brief  Reset autozero mode to normal operation.
 * @tessera expose category=tile name=reset_autozero
 * @param  tile  Initialised tile handle.
 */
void tile_sense_bp_reset_autozero(tile_t *tile);

/**
 * @brief  Enable autorefp mode.
 * @tessera expose category=tile name=set_autorefp
 *
 * Captures current pressure as REF_P for interrupt threshold comparison.
 * Output registers are not affected. Reset with reset_autorefp().
 *
 * @param  tile  Initialised tile handle.
 */
void tile_sense_bp_set_autorefp(tile_t *tile);

/**
 * @brief  Reset autorefp mode to normal operation.
 * @tessera expose category=tile name=reset_autorefp
 * @param  tile  Initialised tile handle.
 */
void tile_sense_bp_reset_autorefp(tile_t *tile);

/**
 * @brief  Set pressure offset for one-point calibration (RPDS registers).
 * @tessera expose category=tile name=set_pressure_offset
 *
 * The offset is in raw LSB units (signed 16-bit) and is subtracted from
 * the measured pressure before output.
 *
 * @param  tile    Initialised tile handle.
 * @param  offset  Raw offset value (two's complement).
 */
void tile_sense_bp_set_pressure_offset(tile_t *tile, int16_t offset);

/**
 * @brief  Read the reference pressure registers (REF_P).
 * @tessera expose category=tile name=get_ref_pressure returns=int
 * @param  tile  Initialised tile handle.
 * @return 16-bit signed reference pressure value.
 */
int16_t tile_sense_bp_get_ref_pressure(tile_t *tile);

#endif /* TILE_SENSE_BP_H */
