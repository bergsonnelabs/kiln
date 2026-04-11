/**
 * @file   tile_drive_dc_h.h
 * @brief  H-bridge DC motor driver for the Drive.DC.H tile (rev a).
 *
 * Embeds the TI DRV8214, a full H-bridge motor driver with integrated
 * current sensing, stall detection, and voltage/speed regulation.
 *
 * Key specifications:
 *   - Output:        full H-bridge, 4 A peak / 2 A RMS continuous
 *   - Supply:        1.65-11 V motor supply (VM), 1.65-5.5 V logic (VCC)
 *   - R_DS(on):      240 mOhm (high + low side total)
 *   - Regulation:    voltage or speed, via internal PI loop
 *   - Current sense: internal current mirror (IPROPI), 3 gain settings
 *   - Stall detect:  hardware, configurable via IPROPI/VREF threshold
 *   - Ripple count:  sensorless position/speed via commutation ripples
 *   - Protection:    UVLO, OCP, OVP, thermal shutdown
 *
 * The tile also exposes EN/PH control pads (pads 2, 3) for direct
 * GPIO-based motor control without I2C, and an analog current
 * proportional output on pad 6 (NPROP/IPROPI) that mirrors the
 * motor winding current scaled by the CS_GAIN_SEL setting.
 * External pad control still requires a one-time I2C write to
 * enable the output stage (EN_OUT=1). A future driver revision
 * will add pad-control mode helpers.
 *
 * The DRV8214 integrates a sensorless ripple counting algorithm
 * that counts commutation ripples in the motor current waveform
 * to estimate rotor position and speed without external encoders
 * or Hall sensors. In speed regulation mode (DRIVE_DC_H_MODE_SPEED),
 * the chip's internal PI loop maintains constant motor speed across
 * varying supply voltages and loads. The ripple count and estimated
 * speed can be read at any time via get_ripple_count() / get_speed().
 * Accurate ripple counting requires motor-specific tuning of the
 * INV_R, KMC, and filter parameters (see DRV8214 datasheet
 * section 9.2.3.1).
 *
 * Datasheet: https://www.bergsonne.io/tiles/drive/dc-h
 * IC datasheet: https://www.ti.com/lit/ds/symlink/drv8214.pdf
 *
 * Quick start:
 * @code
 *   #include "core_tiles.h"
 *
 *   tile_t motor;
 *   tile_drive_dc_h_init(core_tiles_pal(&core_i2c3), 0, &motor, NULL);
 *   if (tile_is_ready(&motor)) {
 *       tile_drive_dc_h_forward(&motor);   // drive forward
 *       core_delay_ms(2000);
 *       tile_drive_dc_h_brake(&motor);     // slow-decay brake
 *   }
 * @endcode
 */

#ifndef INC_TILE_DRIVE_DC_H_H_
#define INC_TILE_DRIVE_DC_H_H_

#include "tiles.h"
#include <stdint.h>

/* -------------------------------------------------------------- */
/* Driver version                                                  */
/* -------------------------------------------------------------- */

#define TILE_DRIVE_DC_H_VERSION_MAJOR  3
#define TILE_DRIVE_DC_H_VERSION_MINOR  0
#define TILE_DRIVE_DC_H_VERSION_PATCH  0

TILES_CHECK_VERSION(1, 0);  /* requires tiles.h >= 1.0 */

/* -------------------------------------------------------------- */
/* Instance mapping                                                */
/* -------------------------------------------------------------- */

/**
 * @brief  Instance-to-address mapping for Drive.DC.H.
 *
 * The DRV8214 derives its I2C address from tri-level pins A0 and A1
 * (tied to VCC, GND, or left floating). Up to 9 devices per bus.
 *
 * | Instance | A1   | A0   | 7-bit addr |
 * |----------|------|------|------------|
 * | 0        | Hi-Z | Hi-Z | 0x34       |
 * | 1        | 0    | 0    | 0x30       |
 * | 2        | 0    | Hi-Z | 0x31       |
 * | 3        | 0    | 1    | 0x32       |
 * | 4        | Hi-Z | 0    | 0x33       |
 * | 5        | Hi-Z | 1    | 0x35       |
 * | 6        | 1    | 0    | 0x36       |
 * | 7        | 1    | Hi-Z | 0x37       |
 * | 8        | 1    | 1    | 0x38       |
 */
#define DRV8214_I2C_ADDR_DEFAULT    0x34

/* -------------------------------------------------------------- */
/* DRV8214 register map                                            */
/* -------------------------------------------------------------- */

/* Status registers (read-only) */
#define DRV8214_REG_FAULT           0x00  /**< Fault status */
#define DRV8214_REG_RC_STATUS1      0x01  /**< Ripple speed estimate */
#define DRV8214_REG_RC_STATUS2      0x02  /**< Ripple count [7:0] */
#define DRV8214_REG_RC_STATUS3      0x03  /**< Ripple count [15:8] */
#define DRV8214_REG_REG_STATUS1     0x04  /**< Motor terminal voltage */
#define DRV8214_REG_REG_STATUS2     0x05  /**< Motor current */
#define DRV8214_REG_REG_STATUS3     0x06  /**< Internal duty cycle */

/* Configuration registers */
#define DRV8214_REG_CONFIG0         0x09  /**< Output/fault enables */
#define DRV8214_REG_CONFIG1         0x0A  /**< TINRUSH [7:0] */
#define DRV8214_REG_CONFIG2         0x0B  /**< TINRUSH [15:8] */
#define DRV8214_REG_CONFIG3         0x0C  /**< IMODE, SMODE, INT_VREF */
#define DRV8214_REG_CONFIG4         0x0D  /**< Bridge ctrl, reporting */

/* Regulation control registers */
#define DRV8214_REG_CTRL0           0x0E  /**< Reg mode, soft-start, PWM */
#define DRV8214_REG_CTRL1           0x0F  /**< WSET_VSET target */
#define DRV8214_REG_CTRL2           0x10  /**< OUT_FLT, EXT_DUTY */

/* Ripple counting control registers */
#define DRV8214_REG_RC_CTRL0        0x11  /**< EN_RC, CS_GAIN_SEL */
#define DRV8214_REG_RC_CTRL1        0x12  /**< RC_THR [7:0] */
#define DRV8214_REG_RC_CTRL2        0x13  /**< INV_R_SCALE, KMC_SCALE, RC_THR_SCALE, RC_THR[9:8] */
#define DRV8214_REG_RC_CTRL3        0x14  /**< INV_R */
#define DRV8214_REG_RC_CTRL4        0x15  /**< KMC */

/** @brief  Expected CONFIG3 register value at power-on. */
#define DRV8214_CONFIG3_DEFAULT     0x63

/* -------------------------------------------------------------- */
/* Fault register bit masks                                        */
/* -------------------------------------------------------------- */

#define DRV8214_FAULT_FAULT         0x80  /**< Bit 7 — any fault active */
#define DRV8214_FAULT_STALL         0x20  /**< Bit 5 — motor stall */
#define DRV8214_FAULT_OCP           0x10  /**< Bit 4 — overcurrent */
#define DRV8214_FAULT_OVP           0x08  /**< Bit 3 — overvoltage */
#define DRV8214_FAULT_TSD           0x04  /**< Bit 2 — thermal shutdown */
#define DRV8214_FAULT_NPOR          0x02  /**< Bit 1 — power-on reset */
#define DRV8214_FAULT_CNT_DONE      0x01  /**< Bit 0 — ripple count done */

/* -------------------------------------------------------------- */
/* Regulation mode selection                                       */
/* -------------------------------------------------------------- */

/** Voltage regulation — internal PI loop maintains target motor voltage.
 *  WSET_VSET sets the target; VM_GAIN_SEL selects the voltage range.
 *  Ripple counting is disabled. */
#define DRIVE_DC_H_MODE_VOLTAGE     0

/** Speed regulation — internal PI loop maintains target motor speed
 *  using the ripple counting algorithm. Requires motor_mohm,
 *  ripples_per_rev, and kv_uv_per_rpm in the config struct. */
#define DRIVE_DC_H_MODE_SPEED       1

/** Voltage regulation with ripple counting enabled. Motor runs at the
 *  target voltage while counting commutation ripples for position
 *  tracking. Use get_ripple_count() to read position, get_speed() for
 *  speed estimate. Provide motor_mohm and ripples_per_rev for accurate
 *  counting; kv_uv_per_rpm improves speed estimation but is optional. */
#define DRIVE_DC_H_MODE_RIPPLE_COUNT 2

/* -------------------------------------------------------------- */
/* Public API                                                      */
/* -------------------------------------------------------------- */

/**
 * @brief  Check whether a DRV8214 is present on the I2C bus.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0-8, see mapping table)
 * @return 1 if device ACKs, 0 otherwise
 */
uint8_t tile_drive_dc_h_find(tiles_pal_t* hal, uint8_t instance);

/**
 * Optional init config. Pass NULL for defaults:
 *   - Voltage regulation mode (no ripple counting)
 *   - 0-3.92 V range (VM_GAIN_SEL=1)
 *   - 4 A max current sense (CS_GAIN_SEL=0)
 *   - Full target voltage (WSET_VSET=0xFF)
 *   - Soft-start enabled, 25 kHz PWM
 *   - I2C bridge control, PWM mode
 *
 * For ripple counting, set mode to RIPPLE_COUNT or SPEED and
 * provide motor parameters. The driver computes register values
 * (INV_R, KMC, scaling factors) automatically.
 */
typedef struct {
    uint8_t  mode;     /**< DRIVE_DC_H_MODE_VOLTAGE (0), _SPEED (1),
                            or _RIPPLE_COUNT (2). */
    uint8_t  vm_gain;  /**< Voltage range: 0 = 0-15.7 V, 1 = 0-3.92 V.
                            Use 1 for better resolution at low voltages. */
    uint8_t  cs_gain;  /**< Current sense gain (CS_GAIN_SEL, 0-5):
                            0=4A, 1=2A, 2=1A, 3=0.5A, 4=0.25A, 5=0.125A.
                            Lower gain = lower R_DS(on) = higher efficiency. */
    uint8_t  target;   /**< WSET_VSET: target voltage or speed (0-255).
                            In voltage mode with VM_GAIN_SEL=1:
                              WSET_VSET = target_mV * 255 / 3920.
                            0xFF = full scale. */

    /* ---- Ripple counting (set motor_mohm > 0 to enable tuning) ---- */
    uint16_t motor_mohm;       /**< Motor winding resistance in milliohms.
                                    0 = skip ripple tuning (use chip defaults).
                                    Typical small DC motor: 1000-50000 mohm. */
    uint8_t  ripples_per_rev;  /**< Commutation ripples per revolution.
                                    = LCM(brushes, commutator_segments).
                                    Common values: 3, 5, 6, 7, 12.
                                    0 = default (12). */
    uint16_t kv_uv_per_rpm;    /**< Back-EMF constant in uV/RPM.
                                    Improves speed estimation accuracy.
                                    0 = skip KMC tuning.
                                    Typical small motor: 100-2000 uV/RPM. */
} drive_dc_h_cfg_t;

/**
 * @brief  Initialize the DRV8214 motor driver.
 *
 * Verifies device presence, configures I2C bridge control in PWM
 * mode, sets regulation parameters, and enables the output stage
 * in coast state (motor not spinning). Call forward() or reverse()
 * to start the motor.
 *
 * @param  hal       Platform HAL handle
 * @param  instance  Instance index (0-8, see mapping table)
 * @param  tile      Pointer to tile handle (populated by this function)
 * @param  cfg       Optional config, or NULL for defaults
 */
void tile_drive_dc_h_init(tiles_pal_t* hal, uint8_t instance, tile_t* tile,
                          const drive_dc_h_cfg_t *cfg);

/* ---- Motor control ---- */

/**
 * @brief  Drive motor forward (OUT1=H, OUT2=L).
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_dc_h_forward(tile_t* tile);

/**
 * @brief  Drive motor in reverse (OUT1=L, OUT2=H).
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_dc_h_reverse(tile_t* tile);

/**
 * @brief  Active brake (slow-decay, both low-side FETs on).
 *
 * Motor is actively held. Current recirculates through the
 * low-side FETs, providing strong braking force.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_dc_h_brake(tile_t* tile);

/**
 * @brief  Coast (Hi-Z, all FETs off).
 *
 * Motor freewheels. No braking force is applied. This is the
 * initial state after init().
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_dc_h_coast(tile_t* tile);

/* ---- Regulation ---- */

/**
 * @brief  Set the regulation target (WSET_VSET).
 *
 * In voltage mode: sets target motor terminal voltage.
 *   With VM_GAIN_SEL=0: value * 15700/255 mV (61.6 mV/bit)
 *   With VM_GAIN_SEL=1: value * 3920/255 mV  (15.4 mV/bit)
 *
 * In speed mode: sets target ripple speed.
 *   Target speed = value * W_SCALE rad/s.
 *
 * @param  tile   Pointer to tile handle
 * @param  value  Target setpoint (0-255)
 */
void tile_drive_dc_h_set_target(tile_t* tile, uint8_t value);

/* ---- Monitoring ---- */

/**
 * @brief  Read the raw FAULT register (0x00).
 *
 * Contains FAULT[7], STALL[5], OCP[4], OVP[3], TSD[2],
 * NPOR[1], CNT_DONE[0]. Use the DRV8214_FAULT_* masks to
 * decode individual bits.
 *
 * @param  tile  Pointer to tile handle
 * @return Raw fault byte (0 = no faults)
 */
uint8_t tile_drive_dc_h_get_fault(tile_t* tile);

/**
 * @brief  Clear all latched faults.
 *
 * Sets CLR_FLT in CONFIG0. Clears FAULT, OCP, OVP, TSD,
 * and NPOR bits. The CLR_FLT bit is self-clearing.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_dc_h_clear_fault(tile_t* tile);

/**
 * @brief  Check if a motor stall condition is active.
 *
 * @param  tile  Pointer to tile handle
 * @return 1 if STALL bit is set, 0 otherwise
 */
uint8_t tile_drive_dc_h_is_stalled(tile_t* tile);

/**
 * @brief  Read motor terminal voltage in millivolts.
 *
 * Reads the VMTR register. The reading is proportional to the
 * voltage across OUT1-OUT2 terminals. Valid while driving.
 *
 * @param  tile  Pointer to tile handle
 * @return Motor voltage in mV (e.g. 3300 = 3.3 V)
 */
uint16_t tile_drive_dc_h_get_voltage_mv(tile_t* tile);

/**
 * @brief  Read motor current in milliamps.
 *
 * Reads the IMTR register and converts using the current
 * CS_GAIN_SEL setting. The reading reflects the current
 * flowing through the low-side FETs during drive or brake.
 *
 * @param  tile  Pointer to tile handle
 * @return Motor current in mA
 */
uint16_t tile_drive_dc_h_get_current_ma(tile_t* tile);

/**
 * @brief  Read the ripple speed estimate.
 *
 * Returns the raw SPEED register from the ripple counting
 * algorithm. Value is proportional to motor speed but requires
 * motor-specific calibration for RPM conversion.
 *
 * @param  tile  Pointer to tile handle
 * @return Raw speed estimate (0-255)
 */
uint8_t tile_drive_dc_h_get_speed(tile_t* tile);

/**
 * @brief  Read the 16-bit ripple count.
 *
 * Returns the total number of commutation ripples counted
 * since the last clear. Proportional to rotor position.
 *
 * @param  tile  Pointer to tile handle
 * @return Ripple count (0-65535)
 */
uint16_t tile_drive_dc_h_get_ripple_count(tile_t* tile);

/**
 * @brief  Reset the ripple counter to zero.
 *
 * Sets CLR_CNT in CONFIG0. Also clears CNT_DONE flag.
 * The CLR_CNT bit is self-clearing.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_dc_h_clear_ripple_count(tile_t* tile);

/* ---- Power management ---- */

/**
 * @brief  Disable the output stage (all FETs Hi-Z).
 *
 * Clears EN_OUT in CONFIG0. The device remains on the I2C bus
 * and registers are accessible, but no current flows through
 * the motor. OVP protection remains active in sleep.
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_dc_h_sleep(tile_t* tile);

/**
 * @brief  Re-enable the output stage.
 *
 * Sets EN_OUT in CONFIG0. The bridge returns to the last
 * commanded state (coast/brake/forward/reverse).
 *
 * @param  tile  Pointer to tile handle
 */
void tile_drive_dc_h_wake(tile_t* tile);

#endif /* INC_TILE_DRIVE_DC_H_H_ */
